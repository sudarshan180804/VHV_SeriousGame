"""Author the production HBCT prologue and Techniques 1–2.

This idempotent editor script owns only production content below
/Game/VHV_Stuff/HBCT, HBCT-labelled actors in Lvl_Village_Blockout, and the
production quest defaults on BP_ThirdPersonPlayerController.
"""

import traceback
import unreal
import author_objective_location_tracking as objective_tracking


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


def story_effect(state_name, effect_type, value=0):
    return struct(
        unreal.VHVStoryEffect,
        effect_type=effect_type,
        state_tag=tag(state_name),
        value=value)


def effect(flag_name):
    return story_effect(flag_name, unreal.VHVStoryEffectType.SET_FLAG)


def counter_effect(counter_name, value=1):
    return story_effect(counter_name, unreal.VHVStoryEffectType.ADD_COUNTER, value)


def completion_condition(counter_name, compare_value):
    return struct(
        unreal.VHVStoryConditionSet,
        match_mode=unreal.VHVStoryConditionMatch.ALL,
        conditions=[struct(
            unreal.VHVStoryCondition,
            condition_type=unreal.VHVStoryConditionType.COUNTER_GREATER_OR_EQUAL,
            state_tag=tag(counter_name),
            compare_value=compare_value)])


def flag_not_set_condition(flag_name):
    return struct(
        unreal.VHVStoryConditionSet,
        match_mode=unreal.VHVStoryConditionMatch.ALL,
        conditions=[struct(
            unreal.VHVStoryCondition,
            condition_type=unreal.VHVStoryConditionType.FLAG_NOT_SET,
            state_tag=tag(flag_name))])


def counter_at_least_condition(counter_name, compare_value):
    return struct(
        unreal.VHVStoryConditionSet,
        match_mode=unreal.VHVStoryConditionMatch.ALL,
        conditions=[struct(
            unreal.VHVStoryCondition,
            condition_type=unreal.VHVStoryConditionType.COUNTER_GREATER_OR_EQUAL,
            state_tag=tag(counter_name),
            compare_value=compare_value)])


def ordering_items(items):
    return [
        struct(unreal.OrderingItem, item_id=str(index), item_text=text)
        for index, text in enumerate(items, 1)
    ]


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


def multi_question(question_id, prompt, options):
    return struct(
        unreal.QuestionData,
        question_id=question_id,
        question_text=prompt,
        options=[
            struct(unreal.QuestionOption, option_text=text, is_correct=is_correct)
            for text, is_correct in options
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


def major_stinger(label, title, subtitle="", hold_duration=1.8):
    return struct(
        unreal.VHVMajorQuestStingerData,
        label=label,
        title=title,
        subtitle=subtitle,
        intro_sound=None,
        hold_duration=hold_duration)


def activity(tag_name, title, activity_type, prompt, teaching_data, **properties):
    values = {
        "activity_tag": tag(tag_name),
        "activity_title": title,
        "activity_type": activity_type,
        "narrative_context": "",
        "prompt_text": prompt,
        "correct_feedback": "",
        "incorrect_feedback": "Look for what helps the person find their own reason and next step.",
        "partial_feedback": "You identified part of the idea.",
        "teaching": teaching_data if teaching_data is not None else unreal.TeachingContent(),
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
                "",
                "",
                [],
                unreal.TextbookTeachingCategory.TECHNIQUE),
            major_stinger=major_stinger(
                "TECHNIQUE 1",
                "BUILDING MOTIVATION",
                "HEALTH BEHAVIOR CHANGE")),
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
            ],
            correct_feedback="Good observation. Readiness and personal priorities matter before advice."),
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
                unreal.TextbookTeachingCategory.KEY_IDEA),
            correct_feedback="Exactly. Motivation grows when support connects change to what matters personally."),
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
                "Mali's wish to stay independent and attend her grandchild's graduation is the personal goal that can support change."),
            correct_feedback="Exactly. Mali's own hope for independence and her grandchild's graduation gives change personal meaning."),
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
                "Mali understands the health risk, but she worries that refusing her children's gifts will hurt their feelings. The obstacle is social and emotional."),
            correct_feedback="Correct. The obstacle is her concern for her children's feelings, not a lack of health knowledge."),
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
                "A useful summary names what matters, acknowledges the obstacle, and repeats a clear next action that Mali chose herself."),
            correct_feedback="Exactly. The summary connects Mali's goal and obstacle to the clear first step she chose."),
    ]


def build_goal_setting_activities():
    observation = unreal.TextbookActivityType.OBSERVATION
    single = unreal.TextbookActivityType.SINGLE_CHOICE
    multi = unreal.TextbookActivityType.MULTI_CHOICE
    lesson = unreal.TextbookTeachingCategory.LESSON
    return [
        activity(
            "VHV.Activity.HBCT.GoalSetting.GoalParts", "OBSERVATION", observation,
            "What made Auntie Nuan's plan more concrete?", None,
            evidence_tagging=evidence_tagging(
                "What made Auntie Nuan's plan more concrete?",
                [
                    evidence_card("She identified a behavior to change.", True),
                    evidence_card("She decided how often she would do it.", True),
                    evidence_card("She gave the plan a time frame.", True),
                    evidence_card("She added a specific walking behavior.", True),
                    evidence_card("She said only that she wanted better health.", False),
                    evidence_card("The VHV chose every decision without asking her.", False),
                ],
                "Which summary best captures what made the plan concrete?",
                [
                    evidence_card("A useful goal turns a general intention into a behavior, target, and time frame.", True),
                    evidence_card("A useful goal stays broad so it never needs to be measured.", False),
                ],
                [takeaway_card("REVIEW", "A useful goal turns a general intention into a behavior, target, and time frame.")]),
            correct_feedback="A useful goal turns a general intention into a behavior, target, and time frame."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.WhatIsGoalSetting", "LESSON", observation,
            "Review the meaning of goal setting.",
            teaching(
                "WHAT IS GOAL SETTING?",
                "Goal setting means deciding what health behavior should change and defining a clear target to work toward.",
                [
                    "BEHAVIOR — What will change?",
                    "TARGET — What are we trying to achieve?",
                    "PATH — What practical action moves us toward it?",
                    "Motivation gives energy. Goals give direction.",
                ], lesson)),
        activity(
            "VHV.Activity.HBCT.GoalSetting.ClearerPath", "Compare the Two Villagers", single,
            "Which villager has a clearer path for changing their behavior?", None,
            question=question(
                "HBCT_GOAL_CLEARER_PATH",
                "Which villager has a clearer path for changing their behavior?",
                [
                    "Auntie Nuan, because she has defined actions and a time frame.",
                    "Uncle Chai, because he is trying many different things.",
                    "Both are equally clear because both want better health.",
                ], 0),
            attempt_policy=attempt_policy(
                "Look at whether each person knows exactly what behavior they will do and when.",
                "Wanting better health is useful, but without a defined behavior and target it is difficult to judge progress or stay consistent."),
            correct_feedback="Nuan can compare what she actually does with the target she set. Her goal gives her direction."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.GoalFunction", "Function of a Behavioral Goal", single,
            "What is one important function of a behavioral goal?", None,
            question=question(
                "HBCT_GOAL_FUNCTION",
                "What is one important function of a behavioral goal?",
                [
                    "It gives a target that actual behavior can be compared with.",
                    "It guarantees success.",
                    "It removes every obstacle.",
                    "It allows the VHV to make all decisions for the person.",
                ], 0),
            attempt_policy=attempt_policy(
                "Think about how a person can judge whether their actions match their plan.",
                "A goal does not guarantee success or remove obstacles. It provides a clear reference point for action and progress."),
            correct_feedback="A goal provides direction and a reference point for comparing actual behavior with the intended behavior."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.IdentifyMethods", "Identify Self-Set Goals", multi,
            "Which examples show a SELF-SET goal?", None,
            question=multi_question(
                "HBCT_GOAL_METHODS",
                "Which examples show a SELF-SET goal?",
                [
                    ("The person chooses a suitable target themselves.", True),
                    ("The person is unsure what target is appropriate, so the VHV helps shape one.", False),
                    ("I will reduce iced milk tea to once a week.", True),
                    ("Let's begin with 30 minutes of walking, three days this week.", False),
                ]),
            attempt_policy=attempt_policy(
                "Look for examples where the person chooses the target themselves.",
                "Self-set goals are chosen by the person. Jointly set goals involve the VHV helping shape an appropriate target."),
            correct_feedback="These goals are self-set because the person chooses their own appropriate target. The other examples involve the VHV helping shape the goal."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.UsefulGoals", "Evaluate Useful Goals", multi,
            "Which goals would give someone a clear and realistic direction?", None,
            question=multi_question(
                "HBCT_USEFUL_GOALS",
                "Which goals would give someone a clear and realistic direction?",
                [
                    ("This month I will walk briskly for 30 minutes, 3–5 days per week.", True),
                    ("Within 1 month, I will reduce iced milk tea from every day to once a week.", True),
                    ("I will be healthier.", False),
                    ("I will never eat sugar again starting tomorrow forever.", False),
                ]),
            attempt_policy=attempt_policy(
                "Look for goals that describe a specific action and are realistic enough to follow.",
                "'Be healthier' is too vague to guide behavior. 'Never eat sugar again' is extreme and difficult to sustain. A useful goal gives clear direction while remaining realistic."),
            correct_feedback="The stronger goals describe exactly what the person will do and give them a practical target."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.DiagnoseVagueGoal", "Diagnose a Weak Goal", single,
            "'I will be healthier.' What is the main problem?", None,
            question=question(
                "HBCT_DIAGNOSE_VAGUE_GOAL",
                "'I will be healthier.' What is the main problem?",
                [
                    "It does not say what behavior should actually change.",
                    "It includes too much detail about the behavior.",
                    "It asks for too little improvement.",
                    "It gives the person too much ownership.",
                ], 0),
            attempt_policy=attempt_policy(
                "Ask whether the person would know exactly what action to take.",
                "The statement names a broad outcome but does not identify a behavior the person can perform."),
            correct_feedback="Correct. The goal is too vague to guide a specific action."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.DiagnoseExtremeGoal", "Diagnose an Unrealistic Goal", single,
            "'I will never eat sugar again starting tomorrow.' What is the main problem?", None,
            question=question(
                "HBCT_DIAGNOSE_EXTREME_GOAL",
                "'I will never eat sugar again starting tomorrow.' What is the main problem?",
                [
                    "The target is unnecessarily extreme and may be unrealistic to sustain.",
                    "It does not mention any behavior.",
                    "It gives the person too much time to begin.",
                    "It is too easy to measure.",
                ], 0),
            attempt_policy=attempt_policy(
                "Consider whether the target is realistic enough to maintain.",
                "A target can be specific and still be unhelpful when it is unnecessarily extreme or unrealistic to sustain."),
            correct_feedback="Correct. Useful goals should challenge the person while remaining realistic."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.UsefulGoalCharacteristics", "LESSON", observation,
            "Review the characteristics of a useful goal.",
            teaching(
                "WHAT MAKES A USEFUL GOAL?", "",
                [
                    "SPECIFIC — The behavior is clear.",
                    "CHALLENGING — The goal requires meaningful effort.",
                    "CLEARLY DIRECTED — The person knows what they are working toward.",
                    "SHORT-TERM — There is an achievable target that can be worked on now.",
                    "REALISTIC — The goal is possible for this person and situation.",
                    "A useful goal should guide action, not merely sound ambitious.",
                ], lesson)),
        activity(
            "VHV.Activity.HBCT.GoalSetting.ImproveVagueGoal", "Improve a Vague Goal", single,
            "Which version improves the vague goal 'I will exercise more'?", None,
            question=question(
                "HBCT_IMPROVE_VAGUE_GOAL",
                "Which version improves the vague goal 'I will exercise more'?",
                [
                    "I will walk briskly for 30 minutes after dinner on Monday, Wednesday, and Friday this week.",
                    "I will exercise whenever I can.",
                    "I will become very fit.",
                    "I will exercise every day forever.",
                ], 0),
            attempt_policy=attempt_policy(
                "Look for a version that names the activity, amount, schedule, and immediate time frame.",
                "A useful short-term goal makes the action and schedule clear while keeping the target realistic."),
            correct_feedback="The improved version names the behavior, amount, schedule, and immediate time frame."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.MaliDifficulty", "OBSERVATION", observation,
            "What is still making Mali's change difficult?", None,
            evidence_tagging=evidence_tagging(
                "What is still making Mali's change difficult?",
                [
                    evidence_card("Sweets and sweet drinks are still brought into the home.", True),
                    evidence_card("Her behavior is inconsistent from day to day.", True),
                    evidence_card("She has not yet defined a clear behavioral target.", True),
                    evidence_card("She has a meaningful reason to improve her health.", False),
                    evidence_card("She has completely stopped caring about her health.", False),
                ],
                "Which summary best identifies what Mali still needs?",
                [
                    evidence_card("Mali needs a sufficiently clear target for what she will do consistently.", True),
                    evidence_card("Mali needs someone else to make every decision for her.", False),
                ],
                [takeaway_card("REVIEW", "Mali already has motivation. What she lacks is a sufficiently clear target for what she will do consistently.")]),
            correct_feedback="Mali already has motivation. What she lacks is a sufficiently clear target for what she will do consistently."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.MaliBehaviors", "Choose Mali's Behaviors", multi,
            "Which behaviors can Mali directly work on now?", None,
            question=multi_question(
                "HBCT_MALI_BEHAVIORS",
                "Which behaviors can Mali directly work on now?",
                [
                    ("Reduce sugary drinks / added sugar.", True),
                    ("Increase regular walking.", True),
                    ("Become completely healthy immediately.", False),
                    ("Make her children stop buying every sweet food.", False),
                    ("Guarantee that her blood glucose will never rise again.", False),
                ]),
            attempt_policy=attempt_policy(
                "Choose actions Mali can perform herself now.",
                "A behavioral goal should focus on Mali's own actions rather than immediate health outcomes or other people's behavior."),
            correct_feedback="Behavioral goals should focus on actions Mali can actually perform, not outcomes she cannot completely control."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.MaliGoalBoardA", "Mali's Short-Term Goals", multi,
            "Which statements are SHORT-TERM GOALS in Mali's plan?", None,
            question=multi_question(
                "HBCT_MALI_SHORT_TERM_GOALS",
                "Which statements are SHORT-TERM GOALS in Mali's plan?",
                [
                    ("For the next month, reduce iced milk tea from every day to once a week.", True),
                    ("Stop adding sugar to coffee.", True),
                    ("Reduce sugary drinks and added sugar.", False),
                    ("Increase regular walking.", False),
                ]),
            attempt_policy=attempt_policy(
                "Look for statements that define a concrete target Mali can work on now.",
                "Short-term goals state concrete targets for now; desired behaviors describe the broader actions Mali wants to change."),
            correct_feedback="The selected statements define concrete targets Mali will work on now. The other statements describe the broader behaviors she wants to change."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.MaliGoalBoardB", "Mali's Long-Term Goals", multi,
            "Which statements describe Mali's LONG-TERM GOALS?", None,
            question=multi_question(
                "HBCT_MALI_LONG_TERM_GOALS",
                "Which statements describe Mali's LONG-TERM GOALS?",
                [
                    ("Improve blood glucose control and maintain healthier daily habits.", True),
                    ("Work toward blood glucose below 130 mg/dL within 3 months.", True),
                    ("Walk briskly for about 30 minutes, 3–5 days each week.", False),
                ]),
            attempt_policy=attempt_policy(
                "Look for the health outcomes and longer direction supported by Mali's repeated behaviors.",
                "The walking plan is an immediate behavioral target. Long-term goals describe the outcomes and direction it supports."),
            correct_feedback="The walking plan is an immediate behavioral target. The long-term goals describe the health outcomes and direction those repeated behaviors support."),
        activity(
            "VHV.Activity.HBCT.GoalSetting.FinalTransfer", "Final Transfer Check", single,
            "A villager says: 'I want to exercise more.' What should a VHV help clarify first?", None,
            question=question(
                "HBCT_GOAL_FINAL_TRANSFER",
                "A villager says: 'I want to exercise more.' What should a VHV help clarify first?",
                [
                    "What activity they will do, how much, and over what period.",
                    "Which exercise is the most difficult possible.",
                    "A permanent goal for the rest of their life.",
                    "A reward before they begin.",
                ], 0),
            attempt_policy=attempt_policy(
                "Look for the details that turn a broad intention into an actionable target.",
                "A broad intention needs a defined behavior, amount, and time period before it can guide action."),
            correct_feedback="A broad intention becomes useful when it is translated into a clear behavioral target."),
    ]


def build_role_model_activities():
    observation = unreal.TextbookActivityType.OBSERVATION
    single = unreal.TextbookActivityType.SINGLE_CHOICE
    multi = unreal.TextbookActivityType.MULTI_CHOICE
    ordering = unreal.TextbookActivityType.ORDERING
    dialogue = unreal.TextbookActivityType.DIALOGUE_CHOICE
    topic = "HBCT_M03_ROLE_MODEL"

    live_symbolic = [
        ("LiveSymbolic01", "Aunt Saeng talking directly to a neighbor.", 0, ""),
        ("LiveSymbolic02", "A person demonstrating walking in the park.", 0, ""),
        ("LiveSymbolic03", "A health video showing a successful patient.", 1, ""),
        ("LiveSymbolic04", "A success story described in a health booklet.", 1,
         "Live models can be observed directly. Symbolic models demonstrate behavior through media or representation."),
    ]
    activities = [
        activity(
            "VHV.Activity.HBCT.RoleModel.DiscoveryReview", "Discovery Review", observation,
            "What did these scenes have in common?", None,
            evidence_tagging=evidence_tagging(
                "What did these scenes have in common?",
                [
                    evidence_card("Someone observed another person's behavior.", True),
                    evidence_card("The example made a behavior easier to imagine.", True),
                    evidence_card("Some observers tried or considered adapting what they saw.", True),
                    evidence_card("The example could be a real person or something shown through media.", True),
                    evidence_card("Everyone was directly ordered what to do.", False),
                    evidence_card("Every observer copied the behavior perfectly.", False),
                    evidence_card("Only trained health professionals could be models.", False),
                ],
                "Which statement best summarizes the observations?",
                [
                    evidence_card("People can learn health behavior by observing examples around them.", True),
                    evidence_card("Role modeling requires perfect copying.", False),
                ],
                [takeaway_card("REVIEW", "People can learn health behavior by observing examples around them.")]),
            correct_feedback="People can learn health behavior by observing examples around them."),
        activity(
            "VHV.Activity.HBCT.RoleModel.SomchaiProfile", "Build Somchai's Model Profile", observation,
            "What should we look for in a useful role model for Somchai?", None,
            evidence_tagging=evidence_tagging(
                "What should we look for in a useful role model for Somchai?",
                [
                    evidence_card("Similar age or life situation.", True),
                    evidence_card("Similar health challenge.", True),
                    evidence_card("Change that looks achievable.", True),
                    evidence_card("Someone approachable.", True),
                    evidence_card("Someone Somchai can respect.", True),
                    evidence_card("The most athletic person available.", False),
                    evidence_card("Someone who performs the hardest exercise.", False),
                    evidence_card("Someone completely different from Somchai.", False),
                ],
                "Which principle should guide the search?",
                [
                    evidence_card("The model should make change feel relatable and possible.", True),
                    evidence_card("The model should be the most impressive person available.", False),
                ],
                [takeaway_card("MODEL FIT", "A useful model should make the target think: someone like me can do this.")]),
            correct_feedback="A useful model should make the target think: someone like me can do this."),
        activity(
            "VHV.Activity.HBCT.RoleModel.SelectModel", "Select a Role Model", single,
            "Who would you recommend as Somchai's main role model?", None,
            question=question("HBCT_ROLE_MODEL_SELECT", "Who would you recommend as Somchai's main role model?",
                              ["Aunt Saeng", "The young athlete", "The fitness celebrity"], 0),
            attempt_policy=attempt_policy(
                "Think about who Somchai could realistically relate to and learn from.",
                "Aunt Saeng's change is closer to Somchai's age, health concerns, lifestyle, and starting ability."),
            correct_feedback="Aunt Saeng's change is closer to Somchai's age, health concerns, lifestyle, and starting ability."),
        activity(
            "VHV.Activity.HBCT.RoleModel.ExplainModelFit", "Why Does She Fit?", multi,
            "Why is Aunt Saeng a strong match for Somchai?", None,
            question=multi_question("HBCT_ROLE_MODEL_FIT", "Why is Aunt Saeng a strong match for Somchai?", [
                ("Similar life stage.", True), ("Similar health risk.", True),
                ("Her changes are realistic.", True), ("She is approachable.", True),
                ("She is respected locally.", True), ("Somchai can imagine doing what she did.", True),
                ("She exercises harder than everyone else.", False), ("She is famous.", False),
                ("Her lifestyle is completely different from his.", False),
            ]),
            correct_feedback="The strongest model is not necessarily the most impressive person. The model should make successful change feel believable and achievable."),
        activity(
            "VHV.Activity.HBCT.RoleModel.ReconstructProgression", "Reconstruct the Progression", ordering,
            "Reconstruct how modeled behavior becomes a habit.", None,
            ordering_items=ordering_items([
                "Observe achievable behavior", "Try part of the behavior",
                "Receive encouragement / feedback", "Repeat the behavior",
                "Make it part of daily life"]),
            correct_order=["1", "2", "3", "4", "5"],
            correct_feedback="Role modeling can help someone begin a behavior, strengthen it through repeated practice, and make it easier to use in everyday life."),
        activity(
            "VHV.Activity.HBCT.RoleModel.PlanSession", "Plan the Modeling Session", ordering,
            "Prepare Somchai's role-model session.", None,
            ordering_items=ordering_items([
                "Identify the behavior to learn", "Choose a suitable model", "Prepare the model",
                "Demonstrate the behavior", "Recognize successful demonstration",
                "Let the target person try", "Give feedback if needed"]),
            correct_order=["1", "2", "3", "4", "5", "6", "7"],
            correct_feedback="A role-model session should move from a clear behavior and suitable model to demonstration, practice, reinforcement, and feedback."),
        activity(
            "VHV.Activity.HBCT.RoleModel.SaengRealisticChange", "What Matters in Her Example?", single,
            "Which part of Aunt Saeng's story is most useful for Somchai right now?", None,
            question=question("HBCT_ROLE_MODEL_SAENG_CHANGE",
                              "Which part of Aunt Saeng's story is most useful for Somchai right now?", [
                                  "She found realistic substitutions rather than demanding perfection.",
                                  "She followed the hardest diet possible.",
                                  "She immediately eliminated every sweet food.",
                                  "She copied someone else's diet exactly."], 0),
            correct_feedback="Her example shows a realistic change Somchai can adapt rather than an extreme rule he must copy."),
        activity(
            "VHV.Activity.HBCT.RoleModel.SweetsResponse", "Respond to Somchai", dialogue,
            "How should you respond to Somchai's concern about sweets?", None,
            dialogue_choice=struct(unreal.DialogueChoiceActivityReference,
                                   conversation_id="HBCT_RoleModel_SaengDemo",
                                   choice_node_id="RMSaeng_Choice",
                                   required_for_progress=True)),
        activity(
            "VHV.Activity.HBCT.RoleModel.ImitationFeedback", "Support Imitation", single,
            "What feedback would help Somchai imitate the behavior successfully?", None,
            question=question("HBCT_ROLE_MODEL_FEEDBACK",
                              "What feedback would help Somchai imitate the behavior successfully?", [
                                  "You're doing it wrong.",
                                  "Forget it. Walking isn't for you.",
                                  "Good start. Let's reduce the pace and make it something you can repeat regularly."], 2),
            correct_feedback="Feedback should help the person adapt the demonstrated behavior into something safe, realistic, and repeatable."),
        activity(
            "VHV.Activity.HBCT.RoleModel.FinalApplication", "Choose a Useful Model", single,
            "Which model would probably be more useful?", None,
            question=question("HBCT_ROLE_MODEL_FINAL",
                              "Which model would probably be more useful?", [
                                  "A similarly aged person with comparable limitations who gradually became active.",
                                  "The competitive runner because they are more impressive.",
                                  "Whoever exercises the hardest.",
                                  "It doesn't matter who the model is."], 0),
            correct_feedback="The strongest model is often someone the target can realistically relate to and learn from."),
    ]
    for suffix, prompt, correct_index, feedback in live_symbolic:
        activities.insert(1 + len([a for a in activities if str(unreal.GameplayTagLibrary.get_tag_name(
            a.get_editor_property("activity_tag"))).split(".")[-1].startswith("LiveSymbolic")]), activity(
                "VHV.Activity.HBCT.RoleModel." + suffix, "Live or Symbolic", single, prompt, None,
                question=question("HBCT_ROLE_MODEL_" + suffix.upper(), prompt,
                                  ["LIVE MODEL", "SYMBOLIC MODEL"], correct_index),
                correct_feedback=feedback))
    for role_activity in activities:
        role_activity.set_editor_property(
            "incorrect_feedback",
            "Review the model's similarity, achievability, and how the target can adapt the behavior.")
        role_activity.set_editor_property(
            "partial_feedback",
            "You identified part of what makes a role model useful.")
    return activities


def configure_level(level):
    motivation_topic = struct(
        unreal.TopicData,
        topic_id="HBCT_M01_BUILDING_MOTIVATION",
        topic_title="Arrival, Observation, and Building Motivation",
        narrative_introduction="Observe village life, then practice helping a person discover their own reason for change.",
        activities=build_activities())
    goal_setting_topic = struct(
        unreal.TopicData,
        topic_id="HBCT_M02_GOAL_SETTING",
        topic_title="Technique 2 — Goal Setting",
        narrative_introduction="Turn a broad health intention into a clear short-term goal and a longer-term direction.",
        activities=build_goal_setting_activities())
    role_model_topic = struct(
        unreal.TopicData,
        topic_id="HBCT_M03_ROLE_MODEL",
        topic_title="Technique 3 — Role Model",
        narrative_introduction="Observe, select, and use a relatable model to support achievable behavior change.",
        activities=build_role_model_activities())
    day = struct(
        unreal.DayData,
        day_number=1,
        day_title="First Day in the Village",
        narrative_role="Production HBCT prologue, Technique 1, and Technique 2",
        introduction_text="Meet the Instructor and begin by observing real life.",
        topics=[motivation_topic, goal_setting_topic, role_model_topic],
        completion_summary="Building Motivation, Goal Setting, and Role Model complete.",
        completion_takeaways=[
            "Motivation comes from what matters personally.",
            "A VHV guides rather than commands.",
            "A small feasible plan turns readiness toward action.",
            "A useful goal turns intention into a clear behavior, target, and time frame.",
            "A useful role model makes change feel relatable, achievable, and adaptable.",
        ])
    level.set_editor_properties({
        "level_number": 1,
        "level_title": "HBCT — Building Motivation",
        "level_description": "Production prologue, market observation, Building Motivation, and Goal Setting content.",
        "day_data": day,
    })


def text_node(node_id, speaker_name, line, next_id="", speaker_tag="", checkpoint="", presentation=None,
              completion_effects=None, activation_conditions=None):
    values = {
        "node_id": node_id,
        "node_type": unreal.VHVDialogueNodeType.TEXT,
        "speaker_name": speaker_name,
        "text": line,
        "choices": [],
        "next_node_id": next_id,
        "is_checkpoint": bool(checkpoint),
        "completion_effects": completion_effects or [],
    }
    if presentation is not None:
        values["presentation"] = presentation
    if speaker_tag:
        values["speaker_tag"] = tag(speaker_tag)
    if checkpoint:
        values["checkpoint_tag"] = tag(checkpoint)
    if activation_conditions is not None:
        values["activation_conditions"] = activation_conditions
    return struct(unreal.DialogueNode, **values)


def learning_node(node_id, activity_tag, next_id="", topic_id="HBCT_M01_BUILDING_MOTIVATION"):
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
            topic_id=topic_id,
            required_for_progress=True),
        completion_effects=[])


def dialogue_choice(option_id, text, next_node_id, is_default=False):
    return struct(
        unreal.DialogueChoiceOption,
        option_id=option_id,
        option_text=text,
        next_node_id=next_node_id,
        is_default=is_default,
        selection_effects=[])


def choice_node(node_id, speaker_name, prompt, choices, speaker_tag=""):
    values = {
        "node_id": node_id,
        "node_type": unreal.VHVDialogueNodeType.CHOICE,
        "speaker_name": speaker_name,
        "text": prompt,
        "choices": choices,
        "next_node_id": "",
        "is_checkpoint": False,
        "completion_effects": [],
    }
    if speaker_tag:
        values["speaker_tag"] = tag(speaker_tag)
    return struct(unreal.DialogueNode, **values)


def one_shot_dialogue(nodes, flag_name, required_flag_name=""):
    conditions = [struct(
        unreal.VHVStoryCondition,
        condition_type=unreal.VHVStoryConditionType.FLAG_NOT_SET,
        state_tag=tag(flag_name))]
    if required_flag_name:
        conditions.append(struct(
            unreal.VHVStoryCondition,
            condition_type=unreal.VHVStoryConditionType.FLAG_SET,
            state_tag=tag(required_flag_name)))
    condition = struct(
        unreal.VHVStoryConditionSet,
        match_mode=unreal.VHVStoryConditionMatch.ALL,
        conditions=conditions)
    for node in nodes:
        node.set_editor_property("activation_conditions", condition)
    return nodes


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
            text_node("Final_03", "Instructor", "The key is not to push people. The key is to help them discover why change matters to them.", speaker_tag="VHV.Participant.Instructor"),
        ])

    goal_topic = "HBCT_M02_GOAL_SETTING"
    specs["DA_Conversation_HBCT_GoalSetting_Arrival"] = (
        "HBCT_GoalSetting_Arrival",
        [
            text_node("GSArrival_01", "Instructor", "You helped Aunt Mali find a reason that matters to her.", "GSArrival_02", "VHV.Participant.Instructor"),
            text_node("GSArrival_02", "Instructor", "But wanting to change is only the beginning.", "GSArrival_03", "VHV.Participant.Instructor"),
            text_node("GSArrival_03", "Player", "She still needs to know what she is actually going to do.", "GSArrival_04"),
            text_node("GSArrival_04", "Instructor", "Exactly. Watch this conversation.", speaker_tag="VHV.Participant.Instructor"),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_Lesson"] = (
        "HBCT_GoalSetting_Lesson",
        [
            text_node("GSLesson_01", "Player", "So the important part wasn't just wanting better health. She made the change specific.", "GSLesson_02", presentation=unreal.VHVDialoguePresentation.THOUGHT),
            text_node("GSLesson_02", "Player", "So motivation gets someone moving. Goals tell them where to go?", "GSLesson_03"),
            text_node("GSLesson_03", "Instructor", "Exactly. Motivation gives energy. Goals give direction.", "GSLesson_04", "VHV.Participant.Instructor"),
            learning_node("GSLesson_04", "VHV.Activity.HBCT.GoalSetting.WhatIsGoalSetting", "GSLesson_05", goal_topic),
            text_node("GSLesson_05", "Instructor", "A definition is easy to remember. The difference becomes clearer when you watch people trying to change.", "GSLesson_06", "VHV.Participant.Instructor"),
            text_node("GSLesson_06", "Instructor", "There are two villagers nearby. Both want better health.", "GSLesson_07", "VHV.Participant.Instructor"),
            text_node("GSLesson_07", "Instructor", "Watch what happens when only one of them has a clear goal.", speaker_tag="VHV.Participant.Instructor"),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_ComparisonReflection"] = (
        "HBCT_GoalSetting_ComparisonReflection",
        [
            learning_node("GSCompare_01", "VHV.Activity.HBCT.GoalSetting.ClearerPath", "GSCompare_02", goal_topic),
            learning_node("GSCompare_02", "VHV.Activity.HBCT.GoalSetting.GoalFunction", "GSCompare_03", goal_topic),
            text_node("GSCompare_03", "Instructor", "Both people wanted better health.", "GSCompare_04", "VHV.Participant.Instructor"),
            text_node("GSCompare_04", "Instructor", "But only one could answer three useful questions.", "GSCompare_05", "VHV.Participant.Instructor"),
            text_node("GSCompare_05", "Instructor", "What am I doing?", "GSCompare_06", "VHV.Participant.Instructor"),
            text_node("GSCompare_06", "Instructor", "How much am I doing?", "GSCompare_07", "VHV.Participant.Instructor"),
            text_node("GSCompare_07", "Instructor", "Am I getting closer to my target?", "GSCompare_08", "VHV.Participant.Instructor"),
            text_node("GSCompare_08", "Player", "So the goal gives them something to compare their actual behavior with.", "GSCompare_09"),
            text_node("GSCompare_09", "Instructor", "Exactly.", speaker_tag="VHV.Participant.Instructor"),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_Methods"] = (
        "HBCT_GoalSetting_Methods",
        [
            text_node("GSMethods_01", "Player", "The patient chose the target themselves.", "GSMethods_02", presentation=unreal.VHVDialoguePresentation.THOUGHT),
            text_node("GSMethods_02", "Player", "This time the VHV helped shape the goal.", "GSMethods_03", presentation=unreal.VHVDialoguePresentation.THOUGHT),
            learning_node("GSMethods_03", "VHV.Activity.HBCT.GoalSetting.IdentifyMethods", "GSMethods_04", goal_topic),
            text_node("GSMethods_04", "Player", "Wouldn't it be easier if the VHV simply told everyone what goal to follow?", "GSMethods_05"),
            text_node("GSMethods_05", "Instructor", "Sometimes guidance is necessary.", "GSMethods_06", "VHV.Participant.Instructor"),
            text_node("GSMethods_06", "Instructor", "But the person still has to live with the goal.", "GSMethods_07", "VHV.Participant.Instructor"),
            text_node("GSMethods_07", "Instructor", "When they can choose an appropriate target themselves, that ownership matters.", "GSMethods_08", "VHV.Participant.Instructor"),
            text_node("GSMethods_08", "Player", "And when they are unsure, we help shape something realistic.", "GSMethods_09"),
            text_node("GSMethods_09", "Instructor", "Exactly.", speaker_tag="VHV.Participant.Instructor"),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_GoodGoals"] = (
        "HBCT_GoalSetting_GoodGoals",
        [
            text_node("GSGood_01", "Instructor", "All four sound like attempts to improve health.", "GSGood_02", "VHV.Participant.Instructor"),
            text_node("GSGood_02", "Instructor", "But are all four useful goals?", "GSGood_03", "VHV.Participant.Instructor"),
            learning_node("GSGood_03", "VHV.Activity.HBCT.GoalSetting.UsefulGoals", "GSGood_04", goal_topic),
            learning_node("GSGood_04", "VHV.Activity.HBCT.GoalSetting.DiagnoseVagueGoal", "GSGood_05", goal_topic),
            learning_node("GSGood_05", "VHV.Activity.HBCT.GoalSetting.DiagnoseExtremeGoal", "GSGood_06", goal_topic),
            learning_node("GSGood_06", "VHV.Activity.HBCT.GoalSetting.UsefulGoalCharacteristics", "GSGood_07", goal_topic),
            learning_node("GSGood_07", "VHV.Activity.HBCT.GoalSetting.ImproveVagueGoal", topic_id=goal_topic),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_MaliCheckIn"] = (
        "HBCT_GoalSetting_MaliCheckIn",
        [
            text_node("GSMaliCheck_01", "Player", "Aunt Mali, how has reducing sugar been going?", "GSMaliCheck_02"),
            text_node("GSMaliCheck_02", "Aunt Mali", "A little better.", "GSMaliCheck_03", "VHV.Participant.AuntMali"),
            text_node("GSMaliCheck_03", "Aunt Mali", "I think more about what I drink now.", "GSMaliCheck_04", "VHV.Participant.AuntMali"),
            text_node("GSMaliCheck_04", "Aunt Mali", "But when my children bring sweets or iced tea, I still have some.", "GSMaliCheck_05", "VHV.Participant.AuntMali"),
            text_node("GSMaliCheck_05", "Player", "That sounds difficult when it is already there in front of you.", "GSMaliCheck_06"),
            text_node("GSMaliCheck_06", "Aunt Mali", "Yes. Some days I do well. Other days I tell myself I will try again tomorrow.", "GSMaliCheck_07", "VHV.Participant.AuntMali"),
            text_node("GSMaliCheck_07", "Player", "Change takes time.", "GSMaliCheck_08"),
            text_node("GSMaliCheck_08", "Player", "Today I'd like to help you make the next step clearer, so you can see what you are actually trying to achieve.", "GSMaliCheck_09"),
            text_node("GSMaliCheck_09", "Aunt Mali", "You mean setting a goal?", "GSMaliCheck_10", "VHV.Participant.AuntMali"),
            text_node("GSMaliCheck_10", "Player", "Exactly."),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_MaliDirection"] = (
        "HBCT_GoalSetting_MaliDirection",
        [
            text_node("GSMaliDirection_01", "Player", "When you think about reducing sugar, what do you want your health to allow you to do?", "GSMaliDirection_02"),
            text_node("GSMaliDirection_02", "Aunt Mali", "I want to stay healthy.", "GSMaliDirection_03", "VHV.Participant.AuntMali"),
            text_node("GSMaliDirection_03", "Aunt Mali", "I want to walk easily and go wherever I need without depending on other people.", "GSMaliDirection_04", "VHV.Participant.AuntMali"),
            text_node("GSMaliDirection_04", "Aunt Mali", "And I want to stay well enough to enjoy time with my family.", "GSMaliDirection_05", "VHV.Participant.AuntMali"),
            text_node("GSMaliDirection_05", "Player", "That is the larger direction.", "GSMaliDirection_06"),
            text_node("GSMaliDirection_06", "Player", "Now let's turn it into something you can actually work on this month.", "GSMaliDirection_07"),
            learning_node("GSMaliDirection_07", "VHV.Activity.HBCT.GoalSetting.MaliBehaviors", topic_id=goal_topic),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_MaliShortTerm"] = (
        "HBCT_GoalSetting_MaliShortTerm",
        [
            text_node("GSMaliGoal_01", "Player", "Let's begin with the sweet drinks. How often are you drinking iced milk tea now?", "GSMaliGoal_02"),
            text_node("GSMaliGoal_02", "Aunt Mali", "Almost every day.", "GSMaliGoal_03", "VHV.Participant.AuntMali"),
            text_node("GSMaliGoal_03", "Player", "What reduction feels challenging, but possible?", "GSMaliGoal_04"),
            text_node("GSMaliGoal_04", "Aunt Mali", "Once a week.", "GSMaliGoal_05", "VHV.Participant.AuntMali"),
            text_node("GSMaliGoal_05", "Player", "For the next month?", "GSMaliGoal_06"),
            text_node("GSMaliGoal_06", "Aunt Mali", "Yes. I think I can try that.", "GSMaliGoal_07", "VHV.Participant.AuntMali"),
            text_node("GSMaliGoal_07", "Player", "What about your coffee?", "GSMaliGoal_08"),
            text_node("GSMaliGoal_08", "Aunt Mali", "I can stop adding sugar.", "GSMaliGoal_09", "VHV.Participant.AuntMali"),
            text_node("GSMaliGoal_09", "Player", "And for activity?", "GSMaliGoal_10"),
            text_node("GSMaliGoal_10", "Aunt Mali", "I could walk.", "GSMaliGoal_11", "VHV.Participant.AuntMali"),
            text_node("GSMaliGoal_11", "Player", "How much would be realistic?", "GSMaliGoal_12"),
            text_node("GSMaliGoal_12", "Aunt Mali", "About thirty minutes.", "GSMaliGoal_13", "VHV.Participant.AuntMali"),
            text_node("GSMaliGoal_13", "Player", "Three days a week to start?", "GSMaliGoal_14"),
            text_node("GSMaliGoal_14", "Aunt Mali", "Maybe three to five days if I'm feeling well.", "GSMaliGoal_15", "VHV.Participant.AuntMali"),
            learning_node("GSMaliGoal_15", "VHV.Activity.HBCT.GoalSetting.MaliGoalBoardA", topic_id=goal_topic),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_MaliReview"] = (
        "HBCT_GoalSetting_MaliReview",
        [
            text_node("GSMaliReview_01", "Aunt Mali", "Seeing it written like that makes it feel much more manageable.", "GSMaliReview_02", "VHV.Participant.AuntMali"),
            text_node("GSMaliReview_02", "Player", "You do not have to change everything at once.", "GSMaliReview_03"),
            text_node("GSMaliReview_03", "Player", "You know what you are working on now.", "GSMaliReview_04"),
            text_node("GSMaliReview_04", "Aunt Mali", "And I can check whether I actually did it.", speaker_tag="VHV.Participant.AuntMali"),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_RoadsidePractice"] = (
        "HBCT_GoalSetting_RoadsidePractice",
        [
            text_node("GSRoad_01", "Villager", "You're helping people with their health goals, right?", "GSRoad_02", "VHV.Participant.GoalSetting.RoadVillager"),
            text_node("GSRoad_02", "Player", "I'm learning how to help people make their plans clearer.", "GSRoad_03"),
            text_node("GSRoad_03", "Villager", "I've been telling myself I should exercise more.", "GSRoad_04", "VHV.Participant.GoalSetting.RoadVillager"),
            text_node("GSRoad_04", "Villager", "But I'm not really sure what that should look like.", "GSRoad_05", "VHV.Participant.GoalSetting.RoadVillager"),
            learning_node("GSRoad_05", "VHV.Activity.HBCT.GoalSetting.FinalTransfer", "GSRoad_06", goal_topic),
            text_node("GSRoad_06", "Villager", "That makes sense. I need something clearer than just saying 'exercise more.'", "GSRoad_07", "VHV.Participant.GoalSetting.RoadVillager"),
            text_node("GSRoad_07", "Player", "Start with something specific and realistic that you can actually follow.", "GSRoad_08"),
            text_node("GSRoad_08", "Villager", "I'll think about what would fit my week. Thanks.", speaker_tag="VHV.Participant.GoalSetting.RoadVillager"),
        ])
    specs["DA_Conversation_HBCT_GoalSetting_FinalDebrief"] = (
        "HBCT_GoalSetting_FinalDebrief",
        [
            text_node("GSFinal_01", "Instructor", "What changed for Aunt Mali today?", "GSFinal_02", "VHV.Participant.Instructor"),
            text_node("GSFinal_02", "Player", "She already had a reason to change.", "GSFinal_03"),
            text_node("GSFinal_03", "Player", "What she needed was a clearer target.", "GSFinal_04"),
            text_node("GSFinal_04", "Instructor", "And who decided that target?", "GSFinal_05", "VHV.Participant.Instructor"),
            text_node("GSFinal_05", "Player", "We built it together.", "GSFinal_06"),
            text_node("GSFinal_06", "Instructor", "Good.", "GSFinal_07", "VHV.Participant.Instructor"),
            text_node("GSFinal_07", "Instructor", "Some people can set an appropriate goal themselves.", "GSFinal_08", "VHV.Participant.Instructor"),
            text_node("GSFinal_08", "Instructor", "Others need help turning a broad intention into something realistic.", "GSFinal_09", "VHV.Participant.Instructor"),
            text_node("GSFinal_09", "Player", "Something they can actually do and compare their behavior with.", "GSFinal_10"),
            text_node("GSFinal_10", "Instructor", "Exactly.", "GSFinal_11", "VHV.Participant.Instructor"),
            text_node("GSFinal_11", "Instructor", "Motivation begins change.", "GSFinal_12", "VHV.Participant.Instructor"),
            text_node("GSFinal_12", "Instructor", "Goals make change concrete.", "GSFinal_13", "VHV.Participant.Instructor"),
            text_node("GSFinal_13", "Instructor", "A useful goal tells the person what they are trying to do, and gives them a direction they can follow.", "GSFinal_14", "VHV.Participant.Instructor"),
            text_node("GSFinal_14", "Instructor", "When people know why they want to change, motivation begins. When they know exactly what to do, change becomes possible.", speaker_tag="VHV.Participant.Instructor"),
        ])

    role_topic = "HBCT_M03_ROLE_MODEL"
    specs["DA_Conversation_HBCT_RoleModel_Reveal"] = (
        "HBCT_RoleModel_Reveal",
        [
            text_node("RMReveal_Thought", "Player", "So the example doesn't even have to be standing here.", "RMReveal_Review",
                      presentation=unreal.VHVDialoguePresentation.THOUGHT),
            learning_node("RMReveal_Review", "VHV.Activity.HBCT.RoleModel.DiscoveryReview", "RMReveal_01", role_topic),
            text_node("RMReveal_01", "Instructor", "You saw four different scenes. What connected them?", "RMReveal_02", "VHV.Participant.Instructor"),
            text_node("RMReveal_02", "Player", "Someone was learning from somebody else's example.", "RMReveal_03"),
            text_node("RMReveal_03", "Instructor", "Exactly.", "RMReveal_04", "VHV.Participant.Instructor"),
            text_node("RMReveal_04", "Instructor", "That is role modeling.", "RMReveal_05", "VHV.Participant.Instructor"),
            text_node("RMReveal_05", "Player", "So they just copy what the other person does?", "RMReveal_06"),
            text_node("RMReveal_06", "Instructor", "Not exactly.", "RMReveal_07", "VHV.Participant.Instructor"),
            text_node("RMReveal_07", "Instructor", "They observe.", "RMReveal_08", "VHV.Participant.Instructor"),
            text_node("RMReveal_08", "Instructor", "They think about whether the behavior fits their own situation.", "RMReveal_09", "VHV.Participant.Instructor"),
            text_node("RMReveal_09", "Instructor", "Then they adapt what is useful.", "RMReveal_10", "VHV.Participant.Instructor"),
            text_node("RMReveal_10", "Instructor", "Role modeling is not simply copying. It is observing, relating, and adapting.", "RMReveal_Class1", "VHV.Participant.Instructor"),
            learning_node("RMReveal_Class1", "VHV.Activity.HBCT.RoleModel.LiveSymbolic01", "RMReveal_Class2", role_topic),
            learning_node("RMReveal_Class2", "VHV.Activity.HBCT.RoleModel.LiveSymbolic02", "RMReveal_Class3", role_topic),
            learning_node("RMReveal_Class3", "VHV.Activity.HBCT.RoleModel.LiveSymbolic03", "RMReveal_Class4", role_topic),
            learning_node("RMReveal_Class4", "VHV.Activity.HBCT.RoleModel.LiveSymbolic04", topic_id=role_topic),
        ])
    specs["DA_Conversation_HBCT_RoleModel_Somchai"] = (
        "HBCT_RoleModel_Somchai",
        [
            text_node("RMSomchai_01", "Player", "Uncle Somchai, have you thought about becoming more active?", "RMSomchai_02"),
            text_node("RMSomchai_02", "Uncle Somchai", "Of course.", "RMSomchai_03", "VHV.Participant.UncleSomchai"),
            text_node("RMSomchai_03", "Uncle Somchai", "But look at those young people exercising.", "RMSomchai_04", "VHV.Participant.UncleSomchai"),
            text_node("RMSomchai_04", "Uncle Somchai", "That's for them.", "RMSomchai_05", "VHV.Participant.UncleSomchai"),
            text_node("RMSomchai_05", "Uncle Somchai", "I'm not going to run around like a twenty-year-old.", "RMSomchai_06", "VHV.Participant.UncleSomchai"),
            text_node("RMSomchai_06", "Player", "Would it help to see someone closer to your situation?", "RMSomchai_07"),
            text_node("RMSomchai_07", "Uncle Somchai", "Maybe.", "RMSomchai_08", "VHV.Participant.UncleSomchai"),
            text_node("RMSomchai_08", "Uncle Somchai", "If someone like me really managed it, I'd listen.", speaker_tag="VHV.Participant.UncleSomchai"),
        ])

    athlete_nodes = [
        text_node("RMAthlete_01", "Player", "What does your exercise routine look like?", "RMAthlete_02"),
        text_node("RMAthlete_02", "Young Athlete", "I train almost every day.", "RMAthlete_03", "VHV.Participant.RoleModel.YoungAthlete"),
        text_node("RMAthlete_03", "Young Athlete", "Running, strength work, sometimes two sessions.", "RMAthlete_04", "VHV.Participant.RoleModel.YoungAthlete"),
        text_node("RMAthlete_04", "Young Athlete", "I've been training for years.", "RMAthlete_05", "VHV.Participant.RoleModel.YoungAthlete"),
        text_node("RMAthlete_05", "Player", "Impressive... but very different from Somchai's situation.",
                  presentation=unreal.VHVDialoguePresentation.THOUGHT,
                  completion_effects=[effect("VHV.Story.Flag.HBCT.RoleModel.CandidateAthleteInspected"),
                                      counter_effect("VHV.Story.Counter.HBCT.RoleModel.CandidatesInspected")]),
    ]
    specs["DA_Conversation_HBCT_RoleModel_YoungAthlete"] = (
        "HBCT_RoleModel_YoungAthlete",
        one_shot_dialogue(
            athlete_nodes, "VHV.Story.Flag.HBCT.RoleModel.CandidateAthleteInspected",
            "VHV.Story.Flag.HBCT.RoleModel.SomchaiProfileComplete"))

    saeng_candidate_nodes = [
        text_node("RMSaengCandidate_01", "Player", "Aunt Saeng, how did you start changing your health habits?", "RMSaengCandidate_02"),
        text_node("RMSaengCandidate_02", "Aunt Saeng", "Slowly.", "RMSaengCandidate_03", "VHV.Participant.AuntSaeng"),
        text_node("RMSaengCandidate_03", "Aunt Saeng", "I used to eat a lot of sweet food.", "RMSaengCandidate_04", "VHV.Participant.AuntSaeng"),
        text_node("RMSaengCandidate_04", "Aunt Saeng", "I didn't suddenly become an athlete.", "RMSaengCandidate_05", "VHV.Participant.AuntSaeng"),
        text_node("RMSaengCandidate_05", "Aunt Saeng", "I changed what I ate, chose less-sweet drinks, and started walking every morning.", "RMSaengCandidate_06", "VHV.Participant.AuntSaeng"),
        text_node("RMSaengCandidate_06", "Player", "How long do you walk?", "RMSaengCandidate_07"),
        text_node("RMSaengCandidate_07", "Aunt Saeng", "Usually around thirty minutes.", "RMSaengCandidate_08", "VHV.Participant.AuntSaeng"),
        text_node("RMSaengCandidate_08", "Aunt Saeng", "At first I did less.", speaker_tag="VHV.Participant.AuntSaeng",
                  completion_effects=[effect("VHV.Story.Flag.HBCT.RoleModel.CandidateSaengInspected"),
                                      counter_effect("VHV.Story.Counter.HBCT.RoleModel.CandidatesInspected")]),
    ]
    specs["DA_Conversation_HBCT_RoleModel_SaengCandidate"] = (
        "HBCT_RoleModel_SaengCandidate",
        one_shot_dialogue(
            saeng_candidate_nodes, "VHV.Story.Flag.HBCT.RoleModel.CandidateSaengInspected",
            "VHV.Story.Flag.HBCT.RoleModel.SomchaiProfileComplete"))

    specs["DA_Conversation_HBCT_RoleModel_Prepare"] = (
        "HBCT_RoleModel_Prepare",
        [
            text_node("RMPrepare_01", "Instructor", "Choosing the model is only part of the job.", "RMPrepare_02", "VHV.Participant.Instructor"),
            text_node("RMPrepare_02", "Instructor", "Now you're going to help run the demonstration.", "RMPrepare_03", "VHV.Participant.Instructor"),
            learning_node("RMPrepare_03", "VHV.Activity.HBCT.RoleModel.PlanSession", topic_id=role_topic),
        ])

    specs["DA_Conversation_HBCT_RoleModel_SaengDemo"] = (
        "HBCT_RoleModel_SaengDemo",
        [
            text_node("RMSaeng_01", "Player", "Uncle Somchai, I'd like you to hear how Aunt Saeng started.", "RMSaeng_02"),
            text_node("RMSaeng_02", "Player", "She also had to make changes because of her diabetes risk.", "RMSaeng_03"),
            text_node("RMSaeng_03", "Uncle Somchai", "Really?", "RMSaeng_04", "VHV.Participant.UncleSomchai"),
            text_node("RMSaeng_04", "Uncle Somchai", "What did you change?", "RMSaeng_05", "VHV.Participant.UncleSomchai"),
            text_node("RMSaeng_05", "Aunt Saeng", "I used to eat many sweet foods.", "RMSaeng_06", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_06", "Aunt Saeng", "I began reducing the very sweet ones instead of trying to change everything overnight.", "RMSaeng_07", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_07", "Aunt Saeng", "I chose lower-sugar fruit more often.", "RMSaeng_08", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_08", "Aunt Saeng", "I drank unsweetened herbal drinks.", "RMSaeng_09", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_09", "Aunt Saeng", "I also started choosing more vegetables and higher-fiber foods.", "RMSaeng_10", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_10", "Aunt Saeng", "And less oily food and refined flour.", "RMSaeng_11", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_11", "Uncle Somchai", "So you didn't just stop eating everything you liked?", "RMSaeng_12", "VHV.Participant.UncleSomchai"),
            text_node("RMSaeng_12", "Aunt Saeng", "No.", "RMSaeng_Activity", "VHV.Participant.AuntSaeng"),
            learning_node("RMSaeng_Activity", "VHV.Activity.HBCT.RoleModel.SaengRealisticChange", "RMSaeng_13", role_topic),
            text_node("RMSaeng_13", "Uncle Somchai", "I still don't think I could completely give up sweets.", "RMSaeng_ChoiceActivity", "VHV.Participant.UncleSomchai"),
            learning_node("RMSaeng_ChoiceActivity", "VHV.Activity.HBCT.RoleModel.SweetsResponse", "RMSaeng_Choice", role_topic),
            choice_node("RMSaeng_Choice", "Player", "How should you respond?", [
                dialogue_choice("A", "Then you aren't ready.", "RMSaeng_WrongA"),
                dialogue_choice("B", "You must stop eating them completely.", "RMSaeng_WrongB"),
                dialogue_choice("C", "Listen to what Aunt Saeng replaced them with.", "RMSaeng_Reinforce", True),
            ]),
            text_node("RMSaeng_WrongA", "Aunt Saeng", "Readiness can grow when a change feels possible. Let me explain the substitutions that helped me.", "RMSaeng_Reinforce", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_WrongB", "Aunt Saeng", "An extreme rule would not have worked for me. I began with realistic substitutions.", "RMSaeng_Reinforce", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_Reinforce", "Aunt Saeng", "I replaced some sweet foods and drinks with choices I could realistically keep using.", "RMSaeng_14", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_14", "Aunt Saeng", "The other big change was walking.", "RMSaeng_15", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_15", "Aunt Saeng", "I started with what I could manage.", "RMSaeng_16", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_16", "Aunt Saeng", "Now I usually brisk-walk around thirty minutes in the morning.", "RMSaeng_17", "VHV.Participant.AuntSaeng"),
            text_node("RMSaeng_17", "Uncle Somchai", "Only thirty minutes?", "RMSaeng_18", "VHV.Participant.UncleSomchai"),
            text_node("RMSaeng_18", "Uncle Somchai", "Can that really make a difference?", "RMSaeng_19", "VHV.Participant.UncleSomchai"),
            text_node("RMSaeng_19", "Aunt Saeng", "The important part for me was doing it consistently.", speaker_tag="VHV.Participant.AuntSaeng"),
        ])

    specs["DA_Conversation_HBCT_RoleModel_MidWalk"] = (
        "HBCT_RoleModel_MidWalk",
        [
            text_node("RMMid_01", "Uncle Somchai", "If I'm doing this, shouldn't I walk as fast as possible?", "RMMid_Activity", "VHV.Participant.UncleSomchai"),
            learning_node("RMMid_Activity", "VHV.Activity.HBCT.RoleModel.ImitationFeedback", "RMMid_02", role_topic),
            text_node("RMMid_02", "Uncle Somchai", "This pace feels much better.", "RMMid_03", "VHV.Participant.UncleSomchai"),
            text_node("RMMid_03", "Aunt Saeng", "Exactly.", "RMMid_04", "VHV.Participant.AuntSaeng"),
            text_node("RMMid_04", "Aunt Saeng", "You don't have to walk like me.", "RMMid_05", "VHV.Participant.AuntSaeng"),
            text_node("RMMid_05", "Aunt Saeng", "You have to find the version you can keep doing.", "RMMid_06", "VHV.Participant.AuntSaeng"),
            text_node("RMMid_06", "Aunt Saeng", "You lead this part.", speaker_tag="VHV.Participant.AuntSaeng"),
        ])
    specs["DA_Conversation_HBCT_RoleModel_WalkEnd"] = (
        "HBCT_RoleModel_WalkEnd",
        [
            text_node("RMEnd_01", "Uncle Somchai", "That wasn't as bad as I expected.", "RMEnd_02", "VHV.Participant.UncleSomchai"),
            text_node("RMEnd_02", "Player", "He isn't copying her anymore. He's adapting what he learned.",
                      presentation=unreal.VHVDialoguePresentation.THOUGHT),
        ])
    specs["DA_Conversation_HBCT_RoleModel_Roadside"] = (
        "HBCT_RoleModel_Roadside",
        [
            text_node("RMRoad_01", "Villager", "My mother wants to become more active once her knee feels better.", "RMRoad_02", "VHV.Participant.RoleModel.RoadVillager"),
            text_node("RMRoad_02", "Villager", "I found a video of a competitive runner.", "RMRoad_03", "VHV.Participant.RoleModel.RoadVillager"),
            text_node("RMRoad_03", "Villager", "Would that be a good example for her?", "RMRoad_Activity", "VHV.Participant.RoleModel.RoadVillager"),
            learning_node("RMRoad_Activity", "VHV.Activity.HBCT.RoleModel.FinalApplication", "RMRoad_04", role_topic),
            text_node("RMRoad_04", "Villager", "That makes sense. She needs an example that feels possible for her.", speaker_tag="VHV.Participant.RoleModel.RoadVillager"),
        ])
    specs["DA_Conversation_HBCT_RoleModel_FinalDebrief"] = (
        "HBCT_RoleModel_FinalDebrief",
        [
            text_node("RMFinal_01", "Instructor", "What made Aunt Saeng useful to Somchai?", "RMFinal_02", "VHV.Participant.Instructor"),
            text_node("RMFinal_02", "Player", "He could see himself doing what she had done.", "RMFinal_03"),
            text_node("RMFinal_03", "Instructor", "Exactly.", "RMFinal_04", "VHV.Participant.Instructor"),
            text_node("RMFinal_04", "Instructor", "A role model does more than inspire.", "RMFinal_05", "VHV.Participant.Instructor"),
            text_node("RMFinal_05", "Instructor", "They make a behavior visible.", "RMFinal_06", "VHV.Participant.Instructor"),
            text_node("RMFinal_06", "Instructor", "They make change feel possible.", "RMFinal_07", "VHV.Participant.Instructor"),
            text_node("RMFinal_07", "Instructor", "And the target still has to adapt that example to their own life.", "RMFinal_08", "VHV.Participant.Instructor"),
            text_node("RMFinal_08", "Instructor", "Observe.", "RMFinal_09", "VHV.Participant.Instructor"),
            text_node("RMFinal_09", "Instructor", "Relate.", "RMFinal_10", "VHV.Participant.Instructor"),
            text_node("RMFinal_10", "Instructor", "Adapt.", "RMFinal_11", "VHV.Participant.Instructor"),
            text_node("RMFinal_11", "Instructor", "Try.", "RMFinal_12", "VHV.Participant.Instructor"),
            text_node("RMFinal_12", "Instructor", "Receive feedback.", "RMFinal_13", "VHV.Participant.Instructor"),
            text_node("RMFinal_13", "Instructor", "Repeat.", "RMFinal_14", "VHV.Participant.Instructor"),
            text_node("RMFinal_14", "Instructor", "The best role model is not always the most impressive person. It is often the person someone can realistically see themselves becoming.", speaker_tag="VHV.Participant.Instructor"),
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
        "DA_Ambient_HBCT_GoalSetting_InitialObservation": {
            "id": "HBCT_GoalSetting_InitialObservation",
            "participants": [
                ambient_participant("VHV", "VHV"),
                ambient_participant("Nuan", "AUNTIE NUAN"),
            ],
            "lines": [
                ambient_line("VHV", "Last time you said you wanted to improve your health. What would you like to work on first?", 4.6, look_at=True),
                ambient_line("Nuan", "I drink sweet milk tea almost every afternoon. I also want to lose some weight.", 4.2, look_at=True),
                ambient_line("VHV", "Which change feels possible to begin with?", 3.2, look_at=True),
                ambient_line("Nuan", "I think I can stop drinking it every day.", 3.2, look_at=True),
                ambient_line("VHV", "How often would you like to have it instead?", 3.2, look_at=True),
                ambient_line("Nuan", "Maybe once a week.", 2.4, look_at=True),
                ambient_line("VHV", "Good. For how long would you like to try that?", 3.2, look_at=True),
                ambient_line("Nuan", "For the next month.", 2.4, look_at=True),
                ambient_line("VHV", "And what could you add that would support your health?", 3.6, look_at=True),
                ambient_line("Nuan", "I could walk after dinner.", 2.7, look_at=True),
                ambient_line("VHV", "How much walking feels realistic?", 2.8, look_at=True),
                ambient_line("Nuan", "Thirty minutes, maybe three days each week.", 3.4, look_at=True),
                ambient_line("VHV", "So for the next month: milk tea once a week, and thirty minutes of walking three days each week.", 5.0, look_at=True),
                ambient_line("Nuan", "That sounds much clearer than just saying I want to be healthier.", 4.0, look_at=True),
            ],
            "show_names": True,
            "effects": [effect("VHV.Story.Flag.HBCT.GoalSetting.InitialObservationComplete")],
        },
        "DA_Ambient_HBCT_GoalSetting_ClearGoal": {
            "id": "HBCT_GoalSetting_ClearGoal",
            "participants": [ambient_participant("Nuan", "AUNTIE NUAN")],
            "lines": [
                ambient_line("Nuan", "Today is Tuesday... walking day.", 2.8, thought=True),
                ambient_line("Nuan", "I already had milk tea this week, so I'll choose water today.", 3.8, thought=True),
                ambient_line("Nuan", "Three walking days this week. I know exactly what I'm trying to do.", 4.0, thought=True),
            ],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.GoalSetting.ComparisonObserved")],
        },
        "DA_Ambient_HBCT_GoalSetting_NoGoal": {
            "id": "HBCT_GoalSetting_NoGoal",
            "participants": [ambient_participant("Chai", "UNCLE CHAI")],
            "lines": [
                ambient_line("Chai", "I really should get healthier.", 2.7, thought=True),
                ambient_line("Chai", "Maybe I'll walk today... or perhaps tomorrow.", 3.2, thought=True),
                ambient_line("Chai", "I tried eating less yesterday. Maybe I should stop rice instead?", 3.8, thought=True),
                ambient_line("Chai", "I don't really know what I'm supposed to be doing.", 3.4, thought=True),
            ],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.GoalSetting.ComparisonObserved")],
        },
        "DA_Ambient_HBCT_GoalSetting_SelfSet": {
            "id": "HBCT_GoalSetting_SelfSet",
            "participants": [
                ambient_participant("VHV", "VHV"),
                ambient_participant("Patient", "VILLAGER"),
            ],
            "lines": [
                ambient_line("VHV", "What change would you like to begin with?", 3.0, look_at=True),
                ambient_line("Patient", "I drink iced milk tea every day. I want to reduce it to once a week.", 4.2, look_at=True),
                ambient_line("VHV", "Does that feel achievable to you?", 2.8, look_at=True),
                ambient_line("Patient", "Yes. That's the goal I want to try.", 3.0, look_at=True),
            ],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.GoalSetting.MethodsObserved")],
        },
        "DA_Ambient_HBCT_GoalSetting_Joint": {
            "id": "HBCT_GoalSetting_Joint",
            "participants": [
                ambient_participant("VHV", "VHV"),
                ambient_participant("Patient", "VILLAGER"),
            ],
            "lines": [
                ambient_line("VHV", "What activity goal would you like to try this week?", 3.4, look_at=True),
                ambient_line("Patient", "I don't know. I hardly exercise. Maybe I should walk every day?", 4.0, look_at=True),
                ambient_line("VHV", "Starting every day may be difficult if you are not walking regularly yet.", 4.0, look_at=True),
                ambient_line("Patient", "Then what would be a good beginning?", 2.8, look_at=True),
                ambient_line("VHV", "How would you feel about brisk walking for thirty minutes, three days this week?", 4.4, look_at=True),
                ambient_line("Patient", "Three days sounds possible. I can try that.", 3.2, look_at=True),
            ],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.GoalSetting.MethodsObserved")],
        },
        "DA_Ambient_HBCT_RoleModel_Exercise": {
            "id": "HBCT_RoleModel_Exercise",
            "participants": [ambient_participant("Observer", "YOUNG OBSERVER"),
                             ambient_participant("Model", "FIT VILLAGER")],
            "lines": [
                ambient_line("Observer", "That doesn't look as difficult as I thought.", 3.0, look_at=True),
                ambient_line("Model", "Good start. Don't worry about doing everything at once.", 3.8, look_at=True),
                ambient_line("Model", "Try this part first.", 2.5, look_at=True),
                ambient_line("Observer", "All right. I can start with that.", 2.8, look_at=True),
            ], "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.Observations")],
        },
        "DA_Ambient_HBCT_RoleModel_SimilarExperience": {
            "id": "HBCT_RoleModel_SimilarExperience",
            "participants": [ambient_participant("Experience", "EXPERIENCED WOMAN"),
                             ambient_participant("Listener", "LISTENING WOMAN")],
            "lines": [
                ambient_line("Experience", "My sugar readings used to worry me too.", 3.0, look_at=True),
                ambient_line("Experience", "I didn't change everything overnight.", 3.0, look_at=True),
                ambient_line("Experience", "I started using less sugar, eating more vegetables, and walking regularly.", 4.5, look_at=True),
                ambient_line("Listener", "You had the same problem I have?", 2.8, look_at=True),
                ambient_line("Experience", "Very similar.", 2.0, look_at=True),
                ambient_line("Listener", "Then maybe I could try what you did.", 3.0, look_at=True),
            ], "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.Observations")],
        },
        "DA_Ambient_HBCT_RoleModel_Family": {
            "id": "HBCT_RoleModel_Family",
            "participants": [ambient_participant("Parent", "PARENT"),
                             ambient_participant("Younger", "YOUNGER FAMILY MEMBER")],
            "lines": [
                ambient_line("Parent", "I'll choose water instead of a sugary drink.", 3.0, look_at=True),
                ambient_line("Younger", "I'll have water too.", 2.4, look_at=True),
            ], "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.Observations")],
        },
        "DA_Ambient_HBCT_RoleModel_Symbolic": {
            "id": "HBCT_RoleModel_Symbolic",
            "participants": [ambient_participant("Observer", "YOUNG VILLAGER")],
            "lines": [
                ambient_line("Observer", "That looks amazing. I want to become strong too.", 3.4),
            ], "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.Observations")],
        },
        "DA_Ambient_HBCT_RoleModel_SymbolicCandidate": {
            "id": "HBCT_RoleModel_SymbolicCandidate",
            "participants": [ambient_participant("Observer", "PLAYER")],
            "lines": [
                ambient_line("Observer", "Fitness celebrity — intensive daily training program.", 3.4),
                ambient_line("Observer", "Healthy and successful... but would Somchai see this as something he could realistically do?", 4.8, thought=True),
            ], "show_names": False,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.CandidatesInspected")],
        },
        "DA_Ambient_HBCT_RoleModel_TimelineBefore": {
            "id": "HBCT_RoleModel_TimelineBefore",
            "participants": [ambient_participant("Target", "TARGET VILLAGER")],
            "lines": [ambient_line("Target", "I'd probably fail if I tried.", 2.8, thought=True)],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved")],
        },
        "DA_Ambient_HBCT_RoleModel_TimelineFindsModel": {
            "id": "HBCT_RoleModel_TimelineFindsModel",
            "participants": [ambient_participant("Target", "TARGET VILLAGER"),
                             ambient_participant("Model", "SIMILAR MODEL")],
            "lines": [ambient_line("Target", "She started from where I am.", 2.8, thought=True)],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved")],
        },
        "DA_Ambient_HBCT_RoleModel_TimelineFirstAttempt": {
            "id": "HBCT_RoleModel_TimelineFirstAttempt",
            "participants": [ambient_participant("Target", "TARGET VILLAGER")],
            "lines": [ambient_line("Target", "Ten minutes is enough for today.", 2.8)],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved")],
        },
        "DA_Ambient_HBCT_RoleModel_TimelineRepetition": {
            "id": "HBCT_RoleModel_TimelineRepetition",
            "participants": [ambient_participant("Target", "TARGET VILLAGER"),
                             ambient_participant("Model", "SIMILAR MODEL")],
            "lines": [ambient_line("Target", "I'll walk this part again today.", 2.8)],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved")],
        },
        "DA_Ambient_HBCT_RoleModel_TimelineNewRoutine": {
            "id": "HBCT_RoleModel_TimelineNewRoutine",
            "participants": [ambient_participant("Target", "TARGET VILLAGER")],
            "lines": [ambient_line("Target", "This is becoming part of my morning.", 3.0)],
            "show_names": True,
            "effects": [counter_effect("VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved")],
        },
        "DA_Ambient_HBCT_RoleModel_WalkAlong": {
            "id": "HBCT_RoleModel_WalkAlong",
            "participants": [ambient_participant("Saeng", "AUNT SAENG"),
                             ambient_participant("Somchai", "UNCLE SOMCHAI")],
            "lines": [
                ambient_line("Saeng", "Starting slowly made it easier for me to keep going.", 2.6),
                ambient_line("Somchai", "I thought I had to do a lot at once.", 2.5),
                ambient_line("Saeng", "Even a short walk is a good place to begin.", 2.6),
                ambient_line("Somchai", "That feels manageable.", 2.2),
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
            "completion_effects": data.get("effects", []),
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


def npc_move(participant_tag, destination_location_tag, face_destination_rotation=False,
             move_speed_override=0.0):
    return struct(
        unreal.VHVQuestNPCMoveRequest,
        participant_tag=tag(participant_tag),
        destination_location_tag=tag(destination_location_tag),
        face_destination_rotation=face_destination_rotation,
        move_speed_override=move_speed_override)


def npc_move_stage(*moves, ambient_conversation_receiver=""):
    return struct(
        unreal.VHVQuestNPCMoveStage,
        moves=list(moves),
        ambient_conversation_receiver_tag=(
            tag(ambient_conversation_receiver)
            if ambient_conversation_receiver else unreal.GameplayTag()))


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
                  world_action_tag=start_action,
                  world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER),
        objective("O03_LearnMotivation", "Learn what motivation building means", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_LearnMotivation"],
                  entry_node_id="Learn_01", auto_start=True),
        objective("O04_ObserveBadGoodDemo", "Observe directive and motivational conversations", q.WORLD_ACTION,
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.Motivation.BadGoodDemo"),
                  world_action_tag=start_action,
                  world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER),
        objective("O05_ReflectOnDemo", "Reflect on which conversation builds readiness", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.Motivation.DemoReflection"), auto_start=True),
        objective("O06_VisitSaeng", "Visit Aunt Saeng", q.REACH_LOCATION,
                  location_tag=tag("VHV.Location.HBCT.SaengHouse")),
        objective("O07_ObserveAppreciation", "Observe appreciation and reflective questions", q.WORLD_ACTION,
                  "VHV.Story.Flag.HBCT.Motivation.SaengObserved",
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.Motivation.Saeng"),
                  world_action_tag=start_action,
                  world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER),
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
                  entry_node_id="Final_01", auto_start=False,
                  completion_npc_moves=[npc_move(
                      "VHV.Participant.Instructor", "VHV.Location.HBCT.HealthPost")]),
    ]
    goal_setting = [
        objective("O01_ReachHealthPost", "Meet the Instructor at the community health post", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_Arrival"],
                  entry_node_id="GSArrival_01", auto_start=False),
        objective("O02_ObserveGoalConversation", "Approach the nearby VHV and observe how an intention becomes a goal", q.WORLD_ACTION,
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.GoalSetting.InitialObservation"),
                  world_action_tag=start_action,
                  world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER),
        objective("O03_IdentifyGoalParts", "Identify what makes the goal concrete", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.GoalSetting.GoalParts"), auto_start=True),
        objective("O04_LearnGoalSetting", "Talk to the Instructor about what goal setting means", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_Lesson"],
                  entry_node_id="GSLesson_01", auto_start=False),
        objective("O05_ComparePatients", "Visit both nearby villagers and compare a person with a goal and one without", q.CUSTOM_EVENT,
                  "VHV.Story.Flag.HBCT.GoalSetting.ComparisonComplete",
                  custom_event_tag=tag("VHV.CustomEvent.HBCT.GoalSetting.ComparisonObserved"),
                  completion_conditions=completion_condition(
                      "VHV.Story.Counter.HBCT.GoalSetting.ComparisonObserved", 2)),
        objective("O06_ReflectOnDirection", "Talk to the Instructor about why goals matter", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_ComparisonReflection"],
                  entry_node_id="GSCompare_01", auto_start=False),
        objective("O07_ObserveGoalMethods", "Visit both demonstration areas and observe two ways of setting goals", q.CUSTOM_EVENT,
                  custom_event_tag=tag("VHV.CustomEvent.HBCT.GoalSetting.MethodsObserved"),
                  completion_conditions=completion_condition(
                      "VHV.Story.Counter.HBCT.GoalSetting.MethodsObserved", 2)),
        objective("O08_IdentifyGoalMethods", "Talk to the Instructor about the two goal-setting methods", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.GoalSetting.MethodsLearned",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_Methods"],
                  entry_node_id="GSMethods_01", auto_start=False),
        objective("O09_EvaluateGoals", "Meet the Instructor at the goal board and evaluate the example goals", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.GoalSetting.GoodGoalsLearned",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_GoodGoals"],
                  entry_node_id="GSGood_01", auto_start=False),
        objective("O10_ReturnToMali", "Visit Aunt Mali and check how reducing sugar is going", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_MaliCheckIn"],
                  entry_node_id="GSMaliCheck_01", auto_start=False),
        objective("O11_FindCurrentDifficulty", "Find what is still making change difficult", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.GoalSetting.MaliDifficulty"), auto_start=True),
        objective("O12_DefineDesiredBehavior", "Decide what behavior Mali wants to change", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_MaliDirection"],
                  entry_node_id="GSMaliDirection_01", auto_start=True),
        objective("O13_BuildShortTermGoal", "Help Mali create a clear short-term goal", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_MaliShortTerm"],
                  entry_node_id="GSMaliGoal_01", auto_start=True),
        objective("O14_LinkLongTermGoal", "Connect the short-term plan to a longer-term health target", q.LEARNING_ACTIVITY,
                  "VHV.Story.Flag.HBCT.GoalSetting.MaliGoalCreated",
                  activity_tag=tag("VHV.Activity.HBCT.GoalSetting.MaliGoalBoardB"), auto_start=True),
        objective("O15_ReviewGoalPlan", "Return to Aunt Mali and review her completed goal plan", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_MaliReview"],
                  entry_node_id="GSMaliReview_01", auto_start=False),
        objective("O15B_RoadsidePractice", "Talk to the villager on the road", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.GoalSetting.RoadVillager"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_RoadsidePractice"],
                  entry_node_id="GSRoad_01", auto_start=False),
        objective("O16_FinalDebrief", "Report back to the Instructor", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.GoalSetting.Completed",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_FinalDebrief"],
                  entry_node_id="GSFinal_01", auto_start=False,
                  completion_npc_moves=[
                      npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.ExercisePark"),
                      npc_move("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengCandidate", True),
                  ]),
    ]
    role_model = [
        objective("O01_ReachExercisePark", "Explore the exercise park", q.REACH_LOCATION,
                  location_tag=tag("VHV.Location.HBCT.RoleModel.Entry")),
        objective("O02_DiscoverRoleModels", "Find examples of people learning from others", q.CUSTOM_EVENT,
                  "VHV.Story.Flag.HBCT.RoleModel.ObservationsComplete",
                  custom_event_tag=tag("VHV.CustomEvent.HBCT.RoleModel.ObservationsComplete"),
                  completion_conditions=completion_condition(
                      "VHV.Story.Counter.HBCT.RoleModel.Observations", 4)),
        objective("O03_ClassifyModels", "Talk to the Instructor about what you observed", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.RoleModel.ModelsClassified",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  location_tag=tag("VHV.Location.HBCT.ExercisePark"),
                  ensure_participant_present_at_location=True,
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_Reveal"],
                  entry_node_id="RMReveal_Thought", auto_start=False),
        objective("O04_MeetSomchai", "Talk to Uncle Somchai", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.UncleSomchai"),
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_Somchai"],
                  entry_node_id="RMSomchai_01", auto_start=False),
        objective("O05_UnderstandSomchai", "Find out what kind of role model could help Somchai", q.LEARNING_ACTIVITY,
                  "VHV.Story.Flag.HBCT.RoleModel.SomchaiProfileComplete",
                  activity_tag=tag("VHV.Activity.HBCT.RoleModel.SomchaiProfile"), auto_start=True),
        objective("O06_InspectCandidates", "Meet the possible role models", q.CUSTOM_EVENT,
                  custom_event_tag=tag("VHV.CustomEvent.HBCT.RoleModel.CandidatesInspected"),
                  ensure_participant_present_at_location=True,
                  activation_npc_readiness=[npc_move(
                      "VHV.Participant.AuntSaeng",
                      "VHV.Location.HBCT.RoleModel.SaengCandidate", True)],
                  completion_conditions=completion_condition(
                      "VHV.Story.Counter.HBCT.RoleModel.CandidatesInspected", 3)),
        objective("O07_SelectRoleModel", "Recommend a role model for Somchai", q.LEARNING_ACTIVITY,
                  "VHV.Story.Flag.HBCT.RoleModel.ModelSelected",
                  activity_tag=tag("VHV.Activity.HBCT.RoleModel.SelectModel"), auto_start=True),
        objective("O08_ExplainModelFit", "Identify why the model is suitable", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.RoleModel.ExplainModelFit"), auto_start=True,
                  completion_npc_moves=[
                      npc_move("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengDemo", True),
                      npc_move("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiDemo", True),
                  ]),
        objective("O09_SeeChangeOverTime", "Follow how role modeling can influence behavior over time", q.CUSTOM_EVENT,
                  custom_event_tag=tag("VHV.CustomEvent.HBCT.RoleModel.TimelineComplete"),
                  completion_conditions=completion_condition(
                      "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", 5)),
        objective("O10_ReconstructModeling", "Reconstruct how modeled behavior becomes a habit", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.RoleModel.ReconstructProgression"), auto_start=True),
        objective("O11_PrepareDemonstration", "Prepare Somchai's role-model session", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  location_tag=tag("VHV.Location.HBCT.ExercisePark"),
                  ensure_participant_present_at_location=True,
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_Prepare"],
                  entry_node_id="RMPrepare_01", auto_start=False),
        objective("O12_ObserveSaeng", "Listen to Aunt Saeng's experience", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.RoleModel.DemonstrationComplete",
                  participant_tag=tag("VHV.Participant.AuntSaeng"),
                  ensure_participant_present_at_location=True,
                  activation_npc_readiness=[
                      npc_move("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengDemo", True),
                      npc_move("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiDemo", True),
                  ],
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_SaengDemo"],
                  entry_node_id="RMSaeng_01", auto_start=False),
        objective("O13_SupportImitation", "Help Somchai try the behavior", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.UncleSomchai"),
                  location_tag=tag("VHV.Location.HBCT.RoleModel.SomchaiDemo"),
                  ensure_participant_present_at_location=True,
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_MidWalk"],
                  entry_node_id="RMMid_01", auto_start=False),
        objective("O14_PracticeTogether", "Walk with Aunt Saeng and Somchai", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.RoleModel.ImitationComplete",
                  participant_tag=tag("VHV.Participant.UncleSomchai"),
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_WalkEnd"],
                  entry_node_id="RMEnd_01", auto_start=True,
                  npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
                  npc_move_stages=[
                      npc_move_stage(
                          npc_move("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengWalkMid", True, 180.0),
                          npc_move("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiWalkMid", True, 180.0),
                          ambient_conversation_receiver="VHV.WorldReceiver.HBCT.RoleModel.WalkAlong",
                      ),
                      npc_move_stage(
                          npc_move("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengWalkEnd", True, 180.0),
                          npc_move("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiWalkEnd", True, 180.0),
                      ),
                  ]),
        objective("O15_FinalApplication", "Help another villager choose a useful model", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.RoleModel.RoadVillager"),
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_Roadside"],
                  entry_node_id="RMRoad_01", auto_start=False),
        objective("O16_FinalDebrief", "Talk to the Instructor", q.CONVERSATION,
                  "VHV.Story.Flag.HBCT.RoleModel.Completed",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  location_tag=tag("VHV.Location.HBCT.ExercisePark"),
                  ensure_participant_present_at_location=True,
                  conversation=conversations["DA_Conversation_HBCT_RoleModel_FinalDebrief"],
                  entry_node_id="RMFinal_01", auto_start=False),
    ]
    for tracking_quest_id, tracking_objectives in (
            ("Q_HBCT_01_MOTIVATION", motivation),
            ("Q_HBCT_02_GOAL_SETTING", goal_setting),
            ("Q_HBCT_03_ROLE_MODEL", role_model)):
        objective_tracking.apply_tracking_metadata_to_objectives(
            tracking_quest_id, tracking_objectives)
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
               auto_start_next_quest=True,
               completion_stinger=major_stinger(
                   "QUEST COMPLETE", "BUILDING MOTIVATION", "TECHNIQUE 1", 1.9)),
        struct(unreal.VHVQuestDefinition,
               quest_id="Q_HBCT_02_GOAL_SETTING",
               quest_title="Technique 2 — Goal Setting",
               quest_description="Turn motivation into a clear, feasible health behavior goal.",
               category=unreal.VHVQuestCategory.MAIN_STORY,
               objectives=goal_setting,
               auto_track=True,
               auto_start_next_quest=True,
               start_stinger=major_stinger(
                   "TECHNIQUE 2", "GOAL SETTING", "HEALTH BEHAVIOR CHANGE", 1.9),
               completion_stinger=major_stinger(
                   "QUEST COMPLETE", "GOAL SETTING", "TECHNIQUE 2", 1.9)),
        struct(unreal.VHVQuestDefinition,
               quest_id="Q_HBCT_03_ROLE_MODEL",
               quest_title="Technique 3 — Role Model",
               quest_description="Use relatable live and symbolic examples to make healthy behavior visible and achievable.",
               category=unreal.VHVQuestCategory.MAIN_STORY,
               objectives=role_model,
               auto_track=True,
               auto_start_next_quest=False,
               start_stinger=major_stinger(
                   "TECHNIQUE 3", "ROLE MODEL", "HEALTH BEHAVIOR CHANGE", 1.9),
               completion_stinger=major_stinger(
                   "QUEST COMPLETE", "ROLE MODEL", "TECHNIQUE 3", 1.9)),
    ]
    arc.set_editor_properties({
        "quest_arc_id": "HBCT",
        "quest_arc_title": "Health Behavior Change Techniques",
        "quest_arc_description": "Production HBCT story arc containing the prologue and Techniques 1–3.",
        "associated_level_data": level,
        "quests": quests,
        "initial_npc_moves": [npc_move(
            "VHV.Participant.Instructor", "VHV.Location.HBCT.VillageEntrance")],
        "auto_start_next_quest": True,
    })


def editor_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_actor(label):
    for actor in editor_actors().get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def set_actor_folder(actor, folder):
    if actor and hasattr(actor, "set_folder_path"):
        actor.set_folder_path(folder)
    return actor


def spawn_actor(actor_class, label, location, rotation=None, preserve_existing_transform=False):
    actor = find_actor(label)
    existed = actor is not None
    if not actor:
        actor = editor_actors().spawn_actor_from_class(
            actor_class, unreal.Vector(*location), rotation or unreal.Rotator())
        if not actor:
            raise RuntimeError("Could not spawn {}".format(label))
        actor.set_actor_label(label)
    actor.modify()
    if not existed or not preserve_existing_transform:
        actor.set_actor_location(unreal.Vector(*location), False, True)
        actor.set_actor_rotation(rotation or unreal.Rotator(), False)
    return actor


def spawn_npc(npc_class, label, location, yaw, participant_tag="", preserve_existing_transform=False):
    actor = spawn_actor(npc_class, label, location, unreal.Rotator(0.0, yaw, 0.0),
                        preserve_existing_transform)
    component = actor.get_quest_participant_component()
    component.modify()
    component.set_editor_property("quest_participation_enabled", bool(participant_tag))
    component.set_editor_property("participant_tag", tag(participant_tag) if participant_tag else unreal.GameplayTag())
    return actor


def configure_npc_nameplate(actor, display_name="", show_nameplate=True):
    if not actor:
        raise RuntimeError("Cannot configure a nameplate on a missing NPC")
    component = actor.get_nameplate_component()
    component.modify()
    component.set_editor_properties({
        "nameplate_display_name": display_name,
        "show_nameplate": bool(show_nameplate and display_name),
    })
    return actor


def bind(slot_id, npc):
    return struct(unreal.VHVAmbientParticipantBinding, slot_id=slot_id, npc=npc)


def configure_ambient_actor(label, location, asset, receiver_tag, bindings,
                            explicit_trigger=False, preserve_existing_transform=False):
    actor = spawn_actor(unreal.VHVAmbientConversationActor, label, location,
                        preserve_existing_transform=preserve_existing_transform)
    actor.set_editor_properties({
        "conversation_data": asset,
        "participants": bindings,
        "auto_start_on_begin_play": False,
        "requires_explicit_trigger": explicit_trigger,
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
    existed = find_actor(label) is not None
    actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location,
                        preserve_existing_transform=True)
    actor.set_editor_properties({
        "location_tag": tag(location_tag),
        "enabled": True,
        "trigger_world_action": False,
        "activate_current_objective": False,
    })
    box = actor.get_editor_property("box_component")
    if not existed:
        box.set_box_extent(unreal.Vector(260.0, 260.0, 220.0), True)
    return actor


def configure_semantic_npc_location(label, location, location_tag):
    existed = find_actor(label) is not None
    actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location,
                        preserve_existing_transform=True)
    actor.set_editor_properties({
        "location_tag": tag(location_tag),
        "enabled": False,
        "trigger_world_action": False,
        "activate_current_objective": False,
        "required_active_quest_id": "",
        "required_active_objective_id": "",
    })
    if not existed:
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(300.0, 300.0, 220.0), True)
    return actor


def configure_story_trigger(label, location, extent, location_tag, quest_id,
                            objective_id, receiver_tag, legacy_label="",
                            track_as_objective=True):
    actor = find_actor(label)
    if not actor and legacy_label:
        actor = find_actor(legacy_label)
        if actor:
            actor.modify()
            actor.set_actor_label(label)
    existed = actor is not None
    if not actor:
        actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location,
                            preserve_existing_transform=True)
    actor.modify()
    actor.set_editor_properties({
        "location_tag": tag(location_tag),
        "enabled": True,
        "trigger_world_action": True,
        "world_action_receiver_tag": tag(receiver_tag),
        "world_action_tag": tag("VHV.WorldAction.StartConversation"),
        "trigger_world_action_once": True,
        "track_world_action_as_objective": track_as_objective,
        "activate_current_objective": False,
        "required_active_quest_id": quest_id,
        "required_active_objective_id": objective_id,
    })
    if not existed:
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(*extent), True)
    return actor


def configure_objective_trigger(label, location, extent, location_tag, quest_id, objective_id):
    existed = find_actor(label) is not None
    actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location,
                        preserve_existing_transform=True)
    actor.set_editor_properties({
        "location_tag": tag(location_tag),
        "enabled": True,
        "trigger_world_action": False,
        "activate_current_objective": True,
        "activate_current_objective_once": True,
        "required_active_quest_id": quest_id,
        "required_active_objective_id": objective_id,
    })
    if not existed:
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(*extent), True)
    return actor


def place_trigger_with_group_if_disconnected(trigger, group_actors, offset=(0.0, 0.0, 0.0), max_distance=900.0):
    """Keep manual placement when connected, but repair legacy triggers left far from their scene."""
    valid_actors = [actor for actor in group_actors if actor]
    if not trigger or not valid_actors:
        raise RuntimeError("Cannot place a production trigger without its associated scene actors")
    locations = [actor.get_actor_location() for actor in valid_actors]
    center = unreal.Vector(
        sum(value.x for value in locations) / len(locations),
        sum(value.y for value in locations) / len(locations),
        sum(value.z for value in locations) / len(locations))
    desired = unreal.Vector(center.x + offset[0], center.y + offset[1], center.z + offset[2])
    current = trigger.get_actor_location()
    distance_squared = ((current.x - center.x) ** 2
                        + (current.y - center.y) ** 2
                        + (current.z - center.z) ** 2)
    if distance_squared > max_distance ** 2:
        trigger.modify()
        trigger.set_actor_location(desired, False, True)
    return desired


def create_blockout_prop(label, location, scale):
    existed = find_actor(label) is not None
    actor = spawn_actor(unreal.StaticMeshActor, label, location,
                        preserve_existing_transform=True)
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    if not existed:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def create_role_model_prop(label, location, scale, folder):
    existed = find_actor(label) is not None
    actor = spawn_actor(unreal.StaticMeshActor, label, location,
                        preserve_existing_transform=True)
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    if not existed:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return set_actor_folder(actor, folder)


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


def configure_map(ambient_assets, conversations):
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    role_model_prefixes = (
        "HBCT_RoleModel_",
        "HBCT_Trigger_RoleModel_",
        "HBCT_Location_RoleModel_",
        "HBCT_NavLink_",
    )
    protected_transforms = {}
    for actor in editor_actors().get_all_level_actors():
        label = actor.get_actor_label()
        if label == "HBCT_Location_ExercisePark" or label.startswith(role_model_prefixes):
            continue
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()
        protected_transforms[label] = (
            location.x, location.y, location.z,
            rotation.pitch, rotation.yaw, rotation.roll,
            scale.x, scale.y, scale.z)
    remove_development_ambient_actor()
    obsolete_saeng_approach = find_actor("HBCT_Location_RoleModel_SaengCandidateApproach")
    if obsolete_saeng_approach:
        unreal.log("{} Removing obsolete Aunt Saeng candidate approach waypoint".format(LOG))
        editor_actors().destroy_actor(obsolete_saeng_approach)

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
        participant = instructor.get_quest_participant_component()
        participant.set_editor_properties({
            "quest_participation_enabled": True,
            "participant_tag": tag("VHV.Participant.Instructor"),
        })
    else:
        instructor = spawn_npc(npc_class, "HBCT_Intro_Instructor", (-1710.0, -1800.0, 100.0), -90.0, "VHV.Participant.Instructor")
    instructor_interaction = instructor.get_npc_interaction_component()
    instructor_interaction.modify()
    instructor_interaction.set_editor_properties({
        "default_interaction_prompt": "Talk",
        "use_quest_objective_text_as_prompt": False,
        "interaction_enabled": True,
    })
    instructor_dialogue = instructor.get_dialogue_component()
    instructor_dialogue.modify()
    instructor_dialogue.set_editor_property("default_conversation", None)

    instructor_start = instructor.get_actor_location()
    configure_semantic_npc_location(
        "HBCT_Location_VillageEntrance",
        (instructor_start.x, instructor_start.y, instructor_start.z),
        "VHV.Location.HBCT.VillageEntrance")
    configure_semantic_npc_location(
        "HBCT_Location_HealthPost", (3000.0, -2500.0, 100.0),
        "VHV.Location.HBCT.HealthPost")
    configure_semantic_npc_location(
        "HBCT_Location_GoalSetting_GoodGoalBoard", (3000.0, -2500.0, 100.0),
        "VHV.Location.HBCT.GoalSetting.GoodGoalBoard")
    configure_semantic_npc_location(
        "HBCT_Location_GoalSetting_FinalInstructor", (3000.0, -2500.0, 100.0),
        "VHV.Location.HBCT.GoalSetting.FinalInstructor")
    spawn_actor(unreal.TargetPoint, "HBCT_OpeningCameraHook", (-1750.0, -2000.0, 180.0),
                unreal.Rotator(-8.0, 90.0, 0.0), preserve_existing_transform=True)

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
        market_npcs[slot] = spawn_npc(
            npc_class, label, (x, y, z), yaw, preserve_existing_transform=True)
    configure_location("HBCT_Location_Market", (-780.0, -1470.0, 100.0), "VHV.Location.HBCT.Market")
    configure_ambient_actor(
        "HBCT_Market_SugarDrinkScene", (-780.0, -1470.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_MarketObservation"],
        "VHV.WorldReceiver.HBCT.MarketObservation",
        [bind(slot, market_npcs[slot]) for slot in ("Buyer", "Vendor", "Smoker", "SweetTea", "Rider", "JunkFood")],
        preserve_existing_transform=True)

    prasert = spawn_npc(npc_class, "HBCT_Motivation_Prasert", (0.0, -1120.0, 100.0), 150.0,
                        "VHV.Participant.UnclePrasert", preserve_existing_transform=True)
    legacy_prasert_duplicate = find_actor("HBCT_Motivation_Prasert2")
    if legacy_prasert_duplicate:
        duplicate_participant = legacy_prasert_duplicate.get_quest_participant_component()
        duplicate_participant.modify()
        duplicate_participant.set_editor_properties({
            "quest_participation_enabled": False,
            "participant_tag": unreal.GameplayTag(),
        })
    configure_ambient_actor(
        "HBCT_Motivation_PrasertScene", (0.0, -1120.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_PrasertObservation"],
        "VHV.WorldReceiver.HBCT.Motivation.Prasert",
        [bind("Prasert", prasert)], explicit_trigger=True,
        preserve_existing_transform=True)
    configure_story_trigger(
        "HBCT_Trigger_PrasertObservation", (-80.0, -1300.0, 100.0),
        (260.0, 260.0, 220.0), "VHV.Location.HBCT.PrasertHouse",
        "Q_HBCT_01_MOTIVATION", "O02_ObservePrasert",
        "VHV.WorldReceiver.HBCT.Motivation.Prasert",
        legacy_label="HBCT_Location_PrasertHouse")
    create_blockout_prop("HBCT_Prop_Prasert_TV", (110.0, -1030.0, 70.0), (0.65, 0.12, 0.45))
    create_blockout_prop("HBCT_Prop_Prasert_SweetDrink", (-50.0, -1060.0, 55.0), (0.08, 0.08, 0.25))
    create_blockout_prop("HBCT_Prop_Prasert_SnackPackets", (-15.0, -1035.0, 48.0), (0.25, 0.16, 0.06))

    directive_vhv = spawn_npc(npc_class, "HBCT_Motivation_DirectiveVHV", (390.0, -760.0, 100.0), 0.0,
                              preserve_existing_transform=True)
    directive_patient = spawn_npc(npc_class, "HBCT_Motivation_DirectivePatient", (560.0, -760.0, 100.0), 180.0,
                                  preserve_existing_transform=True)
    motivational_vhv = spawn_npc(npc_class, "HBCT_Motivation_MotivationalVHV", (390.0, -480.0, 100.0), 0.0,
                                 preserve_existing_transform=True)
    motivational_patient = spawn_npc(npc_class, "HBCT_Motivation_MotivationalPatient", (560.0, -480.0, 100.0), 180.0,
                                     preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_Motivation_BadGoodDemo", (475.0, -620.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_BadGoodDemo"],
        "VHV.WorldReceiver.HBCT.Motivation.BadGoodDemo",
        [bind("DirectiveVHV", directive_vhv), bind("DirectivePatient", directive_patient),
         bind("MotivationalVHV", motivational_vhv), bind("MotivationalPatient", motivational_patient)],
        explicit_trigger=True, preserve_existing_transform=True)
    configure_story_trigger(
        "HBCT_Trigger_BadGoodDemo", (245.0, -620.0, 100.0),
        (110.0, 300.0, 220.0), "VHV.Location.HBCT.BadGoodDemo",
        "Q_HBCT_01_MOTIVATION", "O04_ObserveBadGoodDemo",
        "VHV.WorldReceiver.HBCT.Motivation.BadGoodDemo")
    create_blockout_prop("HBCT_Motivation_BadDemo", (475.0, -850.0, 20.0), (0.9, 0.06, 0.2))
    create_blockout_prop("HBCT_Motivation_GoodDemo", (475.0, -390.0, 20.0), (0.9, 0.06, 0.2))

    saeng = spawn_npc(npc_class, "HBCT_Motivation_Saeng", (1010.0, -120.0, 100.0), 180.0,
                      "VHV.Participant.AuntSaeng", preserve_existing_transform=True)
    saeng_vhv = spawn_npc(npc_class, "HBCT_Motivation_SaengVHV", (820.0, -120.0, 100.0), 0.0,
                          preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_Motivation_SaengConversation", (910.0, -120.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_SaengAppreciation"],
        "VHV.WorldReceiver.HBCT.Motivation.Saeng",
        [bind("Saeng", saeng), bind("VHV", saeng_vhv)],
        explicit_trigger=True, preserve_existing_transform=True)
    configure_story_trigger(
        "HBCT_Trigger_SaengAppreciation", (910.0, -300.0, 100.0),
        (260.0, 260.0, 220.0), "VHV.Location.HBCT.SaengHouse",
        "Q_HBCT_01_MOTIVATION", "O07_ObserveAppreciation",
        "VHV.WorldReceiver.HBCT.Motivation.Saeng",
        legacy_label="HBCT_Location_SaengHouse")

    mali = spawn_npc(npc_class, "HBCT_Motivation_Mali", (1450.0, 650.0, 100.0), 180.0,
                     "VHV.Participant.AuntMali", preserve_existing_transform=True)
    configure_location("HBCT_Location_MaliHouse", (1260.0, 650.0, 100.0), "VHV.Location.HBCT.MaliHouse")
    create_blockout_prop("HBCT_Prop_Mali_IcedMilkTea", (1510.0, 600.0, 55.0), (0.08, 0.08, 0.25))
    create_blockout_prop("HBCT_Prop_Mali_Sweets", (1540.0, 635.0, 48.0), (0.28, 0.18, 0.06))

    mali_location = mali.get_actor_location()
    instructor_location = instructor.get_actor_location()
    roadside_location = (
        mali_location.x + (instructor_location.x - mali_location.x) * 0.30,
        mali_location.y + (instructor_location.y - mali_location.y) * 0.30,
        mali_location.z)
    road_villager = spawn_npc(
        npc_class, "HBCT_GoalSetting_RoadVillager", roadside_location, -135.0,
        "VHV.Participant.GoalSetting.RoadVillager", preserve_existing_transform=True)
    road_interaction = road_villager.get_npc_interaction_component()
    road_interaction.modify()
    road_interaction.set_editor_properties({
        "default_interaction_prompt": "Talk",
        "use_quest_objective_text_as_prompt": False,
        "interaction_enabled": True,
    })
    road_dialogue = road_villager.get_dialogue_component()
    road_dialogue.modify()
    road_dialogue.set_editor_property("default_conversation", None)
    road_location = road_villager.get_actor_location()
    configure_semantic_npc_location(
        "HBCT_Location_GoalSetting_RoadsidePractice",
        (road_location.x, road_location.y, road_location.z),
        "VHV.Location.HBCT.GoalSetting.RoadsidePractice")

    # Technique 2 uses a deliberately simple linear staging strip. These transforms
    # are production-editable defaults only; no quest logic depends on them.
    observation_vhv = spawn_npc(
        npc_class, "HBCT_GoalSetting_ObservationVHV", (4000.0, -2580.0, 100.0), 0.0,
        preserve_existing_transform=True)
    auntie_nuan = spawn_npc(
        npc_class, "HBCT_GoalSetting_AuntieNuan", (4200.0, -2580.0, 100.0), 180.0,
        preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_GoalSetting_InitialObservation", (4100.0, -2580.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_GoalSetting_InitialObservation"],
        "VHV.WorldReceiver.HBCT.GoalSetting.InitialObservation",
        [bind("VHV", observation_vhv), bind("Nuan", auntie_nuan)],
        explicit_trigger=True, preserve_existing_transform=True)
    initial_observation_trigger = configure_story_trigger(
        "HBCT_Trigger_GoalSetting_InitialObservation", (4100.0, -2850.0, 100.0),
        (300.0, 240.0, 220.0), "VHV.Location.HBCT.GoalSetting.InitialObservation",
        "Q_HBCT_02_GOAL_SETTING", "O02_ObserveGoalConversation",
        "VHV.WorldReceiver.HBCT.GoalSetting.InitialObservation")
    place_trigger_with_group_if_disconnected(
        initial_observation_trigger, [observation_vhv, auntie_nuan], (0.0, -270.0, 0.0))

    clear_nuan = spawn_npc(
        npc_class, "HBCT_GoalSetting_Nuan_ClearGoal", (5150.0, -2750.0, 100.0), 135.0,
        preserve_existing_transform=True)
    uncle_chai = spawn_npc(
        npc_class, "HBCT_GoalSetting_UncleChai", (5150.0, -2250.0, 100.0), 225.0,
        preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_GoalSetting_ClearGoalScene", (5150.0, -2750.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_GoalSetting_ClearGoal"],
        "VHV.WorldReceiver.HBCT.GoalSetting.ClearGoal", [bind("Nuan", clear_nuan)],
        explicit_trigger=True, preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_GoalSetting_NoGoalScene", (5150.0, -2250.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_GoalSetting_NoGoal"],
        "VHV.WorldReceiver.HBCT.GoalSetting.NoGoal", [bind("Chai", uncle_chai)],
        explicit_trigger=True, preserve_existing_transform=True)
    clear_goal_trigger = configure_story_trigger(
        "HBCT_Trigger_GoalSetting_ClearGoal", (4900.0, -2750.0, 100.0),
        (220.0, 220.0, 220.0), "VHV.Location.HBCT.GoalSetting.ClearGoal",
        "Q_HBCT_02_GOAL_SETTING", "O05_ComparePatients",
        "VHV.WorldReceiver.HBCT.GoalSetting.ClearGoal", track_as_objective=False)
    place_trigger_with_group_if_disconnected(
        clear_goal_trigger, [clear_nuan], (-250.0, 0.0, 0.0))
    no_goal_trigger = configure_story_trigger(
        "HBCT_Trigger_GoalSetting_NoGoal", (4900.0, -2250.0, 100.0),
        (220.0, 220.0, 220.0), "VHV.Location.HBCT.GoalSetting.NoGoal",
        "Q_HBCT_02_GOAL_SETTING", "O05_ComparePatients",
        "VHV.WorldReceiver.HBCT.GoalSetting.NoGoal", track_as_objective=False)
    place_trigger_with_group_if_disconnected(
        no_goal_trigger, [uncle_chai], (-250.0, 0.0, 0.0))

    self_vhv = spawn_npc(
        npc_class, "HBCT_GoalSetting_SelfSetVHV", (6250.0, -2750.0, 100.0), 0.0,
        preserve_existing_transform=True)
    self_patient = spawn_npc(
        npc_class, "HBCT_GoalSetting_SelfSetPatient", (6450.0, -2750.0, 100.0), 180.0,
        preserve_existing_transform=True)
    joint_vhv = spawn_npc(
        npc_class, "HBCT_GoalSetting_JointVHV", (6250.0, -2250.0, 100.0), 0.0,
        preserve_existing_transform=True)
    joint_patient = spawn_npc(
        npc_class, "HBCT_GoalSetting_JointPatient", (6450.0, -2250.0, 100.0), 180.0,
        preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_GoalSetting_SelfSetScene", (6350.0, -2750.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_GoalSetting_SelfSet"],
        "VHV.WorldReceiver.HBCT.GoalSetting.SelfSet",
        [bind("VHV", self_vhv), bind("Patient", self_patient)],
        explicit_trigger=True, preserve_existing_transform=True)
    configure_ambient_actor(
        "HBCT_GoalSetting_JointScene", (6350.0, -2250.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_GoalSetting_Joint"],
        "VHV.WorldReceiver.HBCT.GoalSetting.Joint",
        [bind("VHV", joint_vhv), bind("Patient", joint_patient)],
        explicit_trigger=True, preserve_existing_transform=True)
    self_set_trigger = configure_story_trigger(
        "HBCT_Trigger_GoalSetting_SelfSet", (6000.0, -2750.0, 100.0),
        (220.0, 220.0, 220.0), "VHV.Location.HBCT.GoalSetting.SelfSet",
        "Q_HBCT_02_GOAL_SETTING", "O07_ObserveGoalMethods",
        "VHV.WorldReceiver.HBCT.GoalSetting.SelfSet", track_as_objective=False)
    place_trigger_with_group_if_disconnected(
        self_set_trigger, [self_vhv, self_patient], (-350.0, 0.0, 0.0))
    joint_trigger = configure_story_trigger(
        "HBCT_Trigger_GoalSetting_Joint", (6000.0, -2250.0, 100.0),
        (220.0, 220.0, 220.0), "VHV.Location.HBCT.GoalSetting.Joint",
        "Q_HBCT_02_GOAL_SETTING", "O07_ObserveGoalMethods",
        "VHV.WorldReceiver.HBCT.GoalSetting.Joint", track_as_objective=False)
    place_trigger_with_group_if_disconnected(
        joint_trigger, [joint_vhv, joint_patient], (-350.0, 0.0, 0.0))

    mali_check_in_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_MaliCheckIn", (8500.0, -2500.0, 100.0),
        (320.0, 280.0, 220.0), "VHV.Location.HBCT.GoalSetting.MaliCheckIn",
        "Q_HBCT_02_GOAL_SETTING", "O10_ReturnToMali")
    place_trigger_with_group_if_disconnected(
        mali_check_in_trigger, [mali], (-180.0, -260.0, 0.0))

    mali_plan_review_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_MaliPlanReview", (8500.0, -2500.0, 100.0),
        (260.0, 240.0, 220.0), "VHV.Location.HBCT.GoalSetting.MaliPlanReview",
        "Q_HBCT_02_GOAL_SETTING", "O15_ReviewGoalPlan")
    place_trigger_with_group_if_disconnected(
        mali_plan_review_trigger, [mali], (180.0, -260.0, 0.0))

    expected_goal_trigger_labels = {
        "HBCT_Trigger_GoalSetting_InitialObservation",
        "HBCT_Trigger_GoalSetting_ClearGoal",
        "HBCT_Trigger_GoalSetting_NoGoal",
        "HBCT_Trigger_GoalSetting_SelfSet",
        "HBCT_Trigger_GoalSetting_Joint",
        "HBCT_Trigger_GoalSetting_MaliCheckIn",
        "HBCT_Trigger_GoalSetting_MaliPlanReview",
    }
    for actor in list(editor_actors().get_all_level_actors()):
        label = actor.get_actor_label()
        if (actor.get_class() == unreal.VHVQuestLocationVolume.static_class()
                and label.startswith("HBCT_Trigger_GoalSetting_")
                and label not in expected_goal_trigger_labels):
            unreal.log("{} Removing obsolete Quest 2 trigger {}".format(LOG, label))
            editor_actors().destroy_actor(actor)

    # Technique 3 is staged as a spaced, readable sequence beginning immediately
    # beyond the Quest 2 greybox. NPC capsule centers remain at floor Z=100.
    role_root = "04_RoleModel"
    configure_semantic_npc_location(
        "HBCT_Location_ExercisePark", (7350.0, -1500.0, 100.0),
        "VHV.Location.HBCT.ExercisePark")
    set_actor_folder(find_actor("HBCT_Location_ExercisePark"), role_root + "/Instructor")
    entry = configure_location(
        "HBCT_RoleModel_ExerciseParkEntry", (7000.0, -1500.0, 100.0),
        "VHV.Location.HBCT.RoleModel.Entry")
    set_actor_folder(entry, role_root + "/Observations")

    fit = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_FitVillager", (7350.0, -1850.0, 100.0), 0.0,
        preserve_existing_transform=True), role_root + "/Observations")
    young_observer = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_YoungObserver", (7550.0, -1850.0, 100.0), 180.0,
        preserve_existing_transform=True), role_root + "/Observations")
    configure_ambient_actor(
        "HBCT_RoleModel_ExerciseScene", (7450.0, -1850.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_RoleModel_Exercise"],
        "VHV.WorldReceiver.HBCT.RoleModel.ExerciseObservation",
        [bind("Observer", young_observer), bind("Model", fit)], True, True)
    set_actor_folder(find_actor("HBCT_RoleModel_ExerciseScene"), role_root + "/Observations")
    trigger = configure_story_trigger(
        "HBCT_Trigger_RoleModel_ExerciseObservation", (7450.0, -2110.0, 100.0),
        (260.0, 220.0, 220.0), "VHV.Location.HBCT.RoleModel.ExerciseObservation",
        "Q_HBCT_03_ROLE_MODEL", "O02_DiscoverRoleModels",
        "VHV.WorldReceiver.HBCT.RoleModel.ExerciseObservation", track_as_objective=False)
    set_actor_folder(trigger, role_root + "/Observations")

    experience = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_ExperienceWoman", (7800.0, -1850.0, 100.0), 0.0,
        preserve_existing_transform=True), role_root + "/Observations")
    listener = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_ListeningWoman", (8000.0, -1850.0, 100.0), 180.0,
        preserve_existing_transform=True), role_root + "/Observations")
    configure_ambient_actor(
        "HBCT_RoleModel_SimilarExperienceScene", (7900.0, -1850.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_RoleModel_SimilarExperience"],
        "VHV.WorldReceiver.HBCT.RoleModel.SimilarExperience",
        [bind("Experience", experience), bind("Listener", listener)], True, True)
    set_actor_folder(find_actor("HBCT_RoleModel_SimilarExperienceScene"), role_root + "/Observations")
    trigger = configure_story_trigger(
        "HBCT_Trigger_RoleModel_SimilarExperience", (7900.0, -2110.0, 100.0),
        (260.0, 220.0, 220.0), "VHV.Location.HBCT.RoleModel.SimilarExperience",
        "Q_HBCT_03_ROLE_MODEL", "O02_DiscoverRoleModels",
        "VHV.WorldReceiver.HBCT.RoleModel.SimilarExperience", track_as_objective=False)
    set_actor_folder(trigger, role_root + "/Observations")

    parent = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_Parent", (7350.0, -1350.0, 100.0), 0.0,
        preserve_existing_transform=True), role_root + "/Observations")
    younger = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_YoungerFamilyMember", (7550.0, -1350.0, 100.0), 180.0,
        preserve_existing_transform=True), role_root + "/Observations")
    configure_ambient_actor(
        "HBCT_RoleModel_FamilyScene", (7450.0, -1350.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_RoleModel_Family"],
        "VHV.WorldReceiver.HBCT.RoleModel.Family",
        [bind("Parent", parent), bind("Younger", younger)], True, True)
    set_actor_folder(find_actor("HBCT_RoleModel_FamilyScene"), role_root + "/Observations")
    trigger = configure_story_trigger(
        "HBCT_Trigger_RoleModel_Family", (7450.0, -1090.0, 100.0),
        (260.0, 220.0, 220.0), "VHV.Location.HBCT.RoleModel.Family",
        "Q_HBCT_03_ROLE_MODEL", "O02_DiscoverRoleModels",
        "VHV.WorldReceiver.HBCT.RoleModel.Family", track_as_objective=False)
    set_actor_folder(trigger, role_root + "/Observations")

    symbolic_observer = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_SymbolicObserver", (8000.0, -1350.0, 100.0), 180.0,
        preserve_existing_transform=True), role_root + "/Observations")
    create_role_model_prop("HBCT_RoleModel_SymbolicPoster", (7800.0, -1350.0, 110.0),
                           (0.08, 1.0, 1.2), role_root + "/Observations")
    configure_ambient_actor(
        "HBCT_RoleModel_SymbolicScene", (7900.0, -1350.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_RoleModel_Symbolic"],
        "VHV.WorldReceiver.HBCT.RoleModel.Symbolic", [bind("Observer", symbolic_observer)], True, True)
    set_actor_folder(find_actor("HBCT_RoleModel_SymbolicScene"), role_root + "/Observations")
    trigger = configure_story_trigger(
        "HBCT_Trigger_RoleModel_Symbolic", (7900.0, -1090.0, 100.0),
        (260.0, 220.0, 220.0), "VHV.Location.HBCT.RoleModel.Symbolic",
        "Q_HBCT_03_ROLE_MODEL", "O02_DiscoverRoleModels",
        "VHV.WorldReceiver.HBCT.RoleModel.Symbolic", track_as_objective=False)
    set_actor_folder(trigger, role_root + "/Observations")

    somchai = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_UncleSomchai", (8450.0, -1500.0, 100.0), 180.0,
        "VHV.Participant.UncleSomchai", preserve_existing_transform=True), role_root + "/Somchai")
    athlete = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_YoungAthlete", (8750.0, -1800.0, 100.0), 180.0,
        "VHV.Participant.RoleModel.YoungAthlete", preserve_existing_transform=True), role_root + "/Candidates")
    for npc, conversation_name in (
            (athlete, "DA_Conversation_HBCT_RoleModel_YoungAthlete"),
            (saeng, "DA_Conversation_HBCT_RoleModel_SaengCandidate")):
        interaction = npc.get_npc_interaction_component()
        interaction.modify()
        interaction.set_editor_properties({
            "default_interaction_prompt": "Talk",
            "use_quest_objective_text_as_prompt": False,
            "interaction_enabled": True,
        })
        dialogue_component = npc.get_dialogue_component()
        dialogue_component.modify()
        dialogue_component.set_editor_property("default_conversation", conversations[conversation_name])
    configure_semantic_npc_location(
        "HBCT_Location_RoleModel_SaengCandidate", (9050.0, -1800.0, 100.0),
        "VHV.Location.HBCT.RoleModel.SaengCandidate")
    set_actor_folder(find_actor("HBCT_Location_RoleModel_SaengCandidate"), role_root + "/Candidates")
    symbolic_candidate_observer = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_SymbolicCandidateObserver", (9250.0, -1350.0, 100.0), 180.0,
        preserve_existing_transform=True), role_root + "/Candidates")
    create_role_model_prop("HBCT_RoleModel_SymbolicCandidateDisplay", (9050.0, -1350.0, 110.0),
                           (0.08, 1.0, 1.2), role_root + "/Candidates")
    configure_ambient_actor(
        "HBCT_RoleModel_SymbolicCandidateScene", (9150.0, -1350.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_RoleModel_SymbolicCandidate"],
        "VHV.WorldReceiver.HBCT.RoleModel.SymbolicCandidate",
        [bind("Observer", symbolic_candidate_observer)], True, True)
    set_actor_folder(find_actor("HBCT_RoleModel_SymbolicCandidateScene"), role_root + "/Candidates")
    trigger = configure_story_trigger(
        "HBCT_Trigger_RoleModel_SymbolicCandidate", (9050.0, -1090.0, 100.0),
        (260.0, 220.0, 220.0), "VHV.Location.HBCT.RoleModel.SymbolicCandidate",
        "Q_HBCT_03_ROLE_MODEL", "O06_InspectCandidates",
        "VHV.WorldReceiver.HBCT.RoleModel.SymbolicCandidate", track_as_objective=False)
    set_actor_folder(trigger, role_root + "/Candidates")

    timeline_specs = [
        ("Before", "TimelineBefore", 9500.0, 0),
        ("FindsModel", "TimelineFindsModel", 9800.0, 1),
        ("FirstAttempt", "TimelineFirstAttempt", 10100.0, 2),
        ("Repetition", "TimelineRepetition", 10400.0, 3),
        ("NewRoutine", "TimelineNewRoutine", 10700.0, 4),
    ]
    for suffix, asset_suffix, x, required_count in timeline_specs:
        target_npc = set_actor_folder(spawn_npc(
            npc_class, "HBCT_RoleModel_Timeline{}_Target".format(suffix),
            (x, -1500.0, 100.0), 90.0, preserve_existing_transform=True),
            role_root + "/Timeline")
        bindings = [bind("Target", target_npc)]
        if suffix in ("FindsModel", "Repetition"):
            model_npc = set_actor_folder(spawn_npc(
                npc_class, "HBCT_RoleModel_Timeline{}_Model".format(suffix),
                (x, -1300.0, 100.0), -90.0, preserve_existing_transform=True),
                role_root + "/Timeline")
            bindings.append(bind("Model", model_npc))
        receiver_name = "VHV.WorldReceiver.HBCT.RoleModel.Timeline." + suffix
        configure_ambient_actor(
            "HBCT_RoleModel_Timeline{}Scene".format(suffix), (x, -1500.0, 100.0),
            ambient_assets["DA_Ambient_HBCT_RoleModel_" + asset_suffix],
            receiver_name, bindings, True, True)
        set_actor_folder(find_actor("HBCT_RoleModel_Timeline{}Scene".format(suffix)), role_root + "/Timeline")
        timeline_trigger = configure_story_trigger(
            "HBCT_Trigger_RoleModel_Timeline{}".format(suffix), (x, -1750.0, 100.0),
            (135.0, 200.0, 220.0), "VHV.Location.HBCT.RoleModel.Timeline." + suffix,
            "Q_HBCT_03_ROLE_MODEL", "O09_SeeChangeOverTime",
            receiver_name, track_as_objective=False)
        if required_count:
            timeline_trigger.set_editor_property(
                "trigger_conditions", counter_at_least_condition(
                    "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", required_count))
        set_actor_folder(timeline_trigger, role_root + "/Timeline")

    configure_semantic_npc_location(
        "HBCT_Location_RoleModel_Demo_Saeng", (11100.0, -1400.0, 100.0),
        "VHV.Location.HBCT.RoleModel.SaengDemo")
    configure_semantic_npc_location(
        "HBCT_Location_RoleModel_Demo_Somchai", (11300.0, -1400.0, 100.0),
        "VHV.Location.HBCT.RoleModel.SomchaiDemo")
    for label in ("HBCT_Location_RoleModel_Demo_Saeng", "HBCT_Location_RoleModel_Demo_Somchai"):
        set_actor_folder(find_actor(label), role_root + "/SaengDemo")
    demo_trigger = configure_objective_trigger(
        "HBCT_Trigger_RoleModel_SaengDemo", (11200.0, -1680.0, 100.0),
        (300.0, 240.0, 220.0), "VHV.Location.HBCT.RoleModel.DemonstrationArea",
        "Q_HBCT_03_ROLE_MODEL", "O12_ObserveSaeng")
    set_actor_folder(demo_trigger, role_root + "/SaengDemo")

    walk_locations = [
        ("HBCT_Location_RoleModel_WalkMid_Saeng", (11600.0, -1400.0, 100.0), "VHV.Location.HBCT.RoleModel.SaengWalkMid"),
        ("HBCT_Location_RoleModel_WalkMid_Somchai", (11750.0, -1400.0, 100.0), "VHV.Location.HBCT.RoleModel.SomchaiWalkMid"),
        ("HBCT_Location_RoleModel_WalkEnd_Saeng", (12150.0, -1400.0, 100.0), "VHV.Location.HBCT.RoleModel.SaengWalkEnd"),
        ("HBCT_Location_RoleModel_WalkEnd_Somchai", (12350.0, -1400.0, 100.0), "VHV.Location.HBCT.RoleModel.SomchaiWalkEnd"),
    ]
    for label, location, location_tag in walk_locations:
        configure_semantic_npc_location(label, location, location_tag)
        set_actor_folder(find_actor(label), role_root + "/PracticeWalk")

    walk_along_scene = configure_ambient_actor(
        "HBCT_RoleModel_WalkAlongScene", (4500.0, 4400.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_RoleModel_WalkAlong"],
        "VHV.WorldReceiver.HBCT.RoleModel.WalkAlong",
        [bind("Saeng", saeng), bind("Somchai", somchai)],
        explicit_trigger=False, preserve_existing_transform=True)
    walk_along_scene.set_editor_properties({
        "preserve_participant_behavior": True,
        "player_leave_policy": unreal.VHVAmbientConversationLeavePolicy.CONTINUE,
        "observation_radius": 0.0,
    })
    set_actor_folder(walk_along_scene, role_root + "/PracticeWalk")

    road_villager_role = set_actor_folder(spawn_npc(
        npc_class, "HBCT_RoleModel_RoadVillager", (12700.0, -1400.0, 100.0), 180.0,
        "VHV.Participant.RoleModel.RoadVillager", preserve_existing_transform=True),
        role_root + "/FinalApplication")
    road_role_interaction = road_villager_role.get_npc_interaction_component()
    road_role_interaction.modify()
    road_role_interaction.set_editor_properties({
        "default_interaction_prompt": "Talk",
        "use_quest_objective_text_as_prompt": False,
        "interaction_enabled": True,
    })
    road_villager_role.get_dialogue_component().set_editor_property("default_conversation", None)

    # Authoritative, spaced Quest 3 staging layout. This final pass intentionally
    # updates Quest 3 actors on every run, but never changes Quest 0-2 transforms.
    role_positions = {
        # Exercise Park entry and four observation scenes.
        "HBCT_RoleModel_ExerciseParkEntry": (-900.0, 3000.0, 100.0),
        "HBCT_RoleModel_FitVillager": (100.0, 3000.0, 100.0),
        "HBCT_RoleModel_YoungObserver": (300.0, 3000.0, 100.0),
        "HBCT_RoleModel_ExerciseScene": (200.0, 3000.0, 100.0),
        "HBCT_Trigger_RoleModel_ExerciseObservation": (200.0, 3000.0, 100.0),
        "HBCT_RoleModel_ExperienceWoman": (1200.0, 3000.0, 100.0),
        "HBCT_RoleModel_ListeningWoman": (1400.0, 3000.0, 100.0),
        "HBCT_RoleModel_SimilarExperienceScene": (1300.0, 3000.0, 100.0),
        "HBCT_Trigger_RoleModel_SimilarExperience": (1300.0, 3000.0, 100.0),
        "HBCT_RoleModel_Parent": (2300.0, 3000.0, 100.0),
        "HBCT_RoleModel_YoungerFamilyMember": (2500.0, 3000.0, 100.0),
        "HBCT_RoleModel_FamilyScene": (2400.0, 3000.0, 100.0),
        "HBCT_Trigger_RoleModel_Family": (2400.0, 3000.0, 100.0),
        "HBCT_RoleModel_SymbolicObserver": (2500.0, 4100.0, 100.0),
        "HBCT_RoleModel_SymbolicPoster": (2300.0, 4100.0, 110.0),
        "HBCT_RoleModel_SymbolicScene": (2400.0, 4100.0, 100.0),
        "HBCT_Trigger_RoleModel_Symbolic": (2400.0, 4100.0, 100.0),

        # Persistent Instructor destination, Somchai, and candidates.
        "HBCT_Location_ExercisePark": (-700.0, 4100.0, 100.0),
        "HBCT_RoleModel_UncleSomchai": (0.0, 4100.0, 100.0),
        "HBCT_RoleModel_YoungAthlete": (700.0, 3900.0, 100.0),
        "HBCT_Location_RoleModel_SaengCandidate": (900.0, 4300.0, 100.0),
        "HBCT_RoleModel_SymbolicCandidateDisplay": (1300.0, 4100.0, 110.0),
        "HBCT_RoleModel_SymbolicCandidateObserver": (1500.0, 4100.0, 100.0),
        "HBCT_RoleModel_SymbolicCandidateScene": (1400.0, 4100.0, 100.0),
        "HBCT_Trigger_RoleModel_SymbolicCandidate": (1400.0, 4100.0, 100.0),

        # Five sequential timeline stations.
        "HBCT_RoleModel_TimelineBefore_Target": (-900.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineBeforeScene": (-900.0, 5200.0, 100.0),
        "HBCT_Trigger_RoleModel_TimelineBefore": (-900.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineFindsModel_Target": (0.0, 5100.0, 100.0),
        "HBCT_RoleModel_TimelineFindsModel_Model": (0.0, 5300.0, 100.0),
        "HBCT_RoleModel_TimelineFindsModelScene": (0.0, 5200.0, 100.0),
        "HBCT_Trigger_RoleModel_TimelineFindsModel": (0.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineFirstAttempt_Target": (900.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineFirstAttemptScene": (900.0, 5200.0, 100.0),
        "HBCT_Trigger_RoleModel_TimelineFirstAttempt": (900.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineRepetition_Target": (1800.0, 5100.0, 100.0),
        "HBCT_RoleModel_TimelineRepetition_Model": (1800.0, 5300.0, 100.0),
        "HBCT_RoleModel_TimelineRepetitionScene": (1800.0, 5200.0, 100.0),
        "HBCT_Trigger_RoleModel_TimelineRepetition": (1800.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineNewRoutine_Target": (2700.0, 5200.0, 100.0),
        "HBCT_RoleModel_TimelineNewRoutineScene": (2700.0, 5200.0, 100.0),
        "HBCT_Trigger_RoleModel_TimelineNewRoutine": (2700.0, 5200.0, 100.0),

        # Demonstration, short walking path, and roadside application.
        "HBCT_Location_RoleModel_Demo_Saeng": (3600.0, 4500.0, 100.0),
        "HBCT_Location_RoleModel_Demo_Somchai": (3600.0, 4300.0, 100.0),
        "HBCT_Trigger_RoleModel_SaengDemo": (3600.0, 4400.0, 100.0),
        "HBCT_Location_RoleModel_WalkMid_Saeng": (4200.0, 4500.0, 100.0),
        "HBCT_Location_RoleModel_WalkMid_Somchai": (4200.0, 4300.0, 100.0),
        "HBCT_Location_RoleModel_WalkEnd_Saeng": (4800.0, 4500.0, 100.0),
        "HBCT_Location_RoleModel_WalkEnd_Somchai": (4800.0, 4300.0, 100.0),
        "HBCT_RoleModel_RoadVillager": (5200.0, 5200.0, 100.0),
    }
    for role_label, role_location in role_positions.items():
        role_actor = find_actor(role_label)
        if not role_actor:
            raise RuntimeError("Quest 3 staging actor is missing: {}".format(role_label))
        role_actor.modify()
        role_actor.set_actor_location(unreal.Vector(*role_location), False, True)

    # Movement destinations carry the final authored facing used by requests that
    # opt into destination orientation. The walking route faces east; Saeng faces
    # back toward the candidate cluster after arriving from her house.
    role_rotations = {
        "HBCT_Location_RoleModel_SaengCandidate": -90.0,
        "HBCT_Location_RoleModel_Demo_Saeng": 0.0,
        "HBCT_Location_RoleModel_Demo_Somchai": 0.0,
        "HBCT_Location_RoleModel_WalkMid_Saeng": 0.0,
        "HBCT_Location_RoleModel_WalkMid_Somchai": 0.0,
        "HBCT_Location_RoleModel_WalkEnd_Saeng": 0.0,
        "HBCT_Location_RoleModel_WalkEnd_Somchai": 0.0,
    }
    for role_label, yaw in role_rotations.items():
        find_actor(role_label).set_actor_rotation(unreal.Rotator(0.0, yaw, 0.0), False)

    # These two authored walkers use a natural group-walking pace without changing
    # ambient villagers or the shared NPC movement implementation.
    for walker in (saeng, somchai):
        movement = walker.get_editor_property("character_movement")
        movement.modify()
        movement.set_editor_property("max_walk_speed", 180.0)

    # The blockout house interior and the village road are separate Recast
    # polygons at the doorway. Bridge them with a normal navigation link so AI
    # path following can route through the existing opening without teleporting
    # or using a Saeng-specific movement implementation.
    saeng_exit_nav_link = find_actor("HBCT_NavLink_SaengHouseExit")
    if saeng_exit_nav_link:
        editor_actors().destroy_actor(saeng_exit_nav_link)
    saeng_exit_nav_link = editor_actors().spawn_actor_from_class(
        unreal.NavLinkProxy, unreal.Vector(-3820.0, -1275.0, 0.0))
    saeng_exit_nav_link.set_actor_label("HBCT_NavLink_SaengHouseExit")
    saeng_exit_nav_link.modify()
    saeng_exit_nav_link.set_actor_location(
        unreal.Vector(-3820.0, -1275.0, 0.0), False, True)
    doorway_link = unreal.NavigationLink()
    doorway_link.set_editor_properties({
        "left": unreal.Vector(-80.0, 0.0, 0.0),
        "right": unreal.Vector(80.0, 0.0, 0.0),
        "direction": unreal.NavLinkDirection.BOTH_WAYS,
        "snap_radius": 90.0,
        "snap_height": 140.0,
    })
    saeng_exit_nav_link.set_editor_property("point_links", [doorway_link])
    set_actor_folder(saeng_exit_nav_link, role_root + "/Navigation")

    role_trigger_extents = {
        "HBCT_RoleModel_ExerciseParkEntry": (260.0, 220.0, 220.0),
        "HBCT_Trigger_RoleModel_ExerciseObservation": (250.0, 200.0, 220.0),
        "HBCT_Trigger_RoleModel_SimilarExperience": (250.0, 200.0, 220.0),
        "HBCT_Trigger_RoleModel_Family": (250.0, 200.0, 220.0),
        "HBCT_Trigger_RoleModel_Symbolic": (250.0, 200.0, 220.0),
        "HBCT_Trigger_RoleModel_SymbolicCandidate": (250.0, 200.0, 220.0),
        "HBCT_Trigger_RoleModel_TimelineBefore": (130.0, 180.0, 220.0),
        "HBCT_Trigger_RoleModel_TimelineFindsModel": (130.0, 180.0, 220.0),
        "HBCT_Trigger_RoleModel_TimelineFirstAttempt": (130.0, 180.0, 220.0),
        "HBCT_Trigger_RoleModel_TimelineRepetition": (130.0, 180.0, 220.0),
        "HBCT_Trigger_RoleModel_TimelineNewRoutine": (130.0, 180.0, 220.0),
        "HBCT_Trigger_RoleModel_SaengDemo": (280.0, 220.0, 220.0),
    }
    for trigger_label, extent in role_trigger_extents.items():
        trigger = find_actor(trigger_label)
        trigger.get_editor_property("box_component").set_box_extent(
            unreal.Vector(*extent), True)

    # Semantic destinations are points for NPC movement, not player triggers.
    # Small editor boxes keep the route readable without changing movement logic.
    for location_label in (
            "HBCT_Location_ExercisePark",
            "HBCT_Location_RoleModel_SaengCandidate",
            "HBCT_Location_RoleModel_Demo_Saeng",
            "HBCT_Location_RoleModel_Demo_Somchai",
            "HBCT_Location_RoleModel_WalkMid_Saeng",
            "HBCT_Location_RoleModel_WalkMid_Somchai",
            "HBCT_Location_RoleModel_WalkEnd_Saeng",
            "HBCT_Location_RoleModel_WalkEnd_Somchai"):
        find_actor(location_label).get_editor_property("box_component").set_box_extent(
            unreal.Vector(70.0, 70.0, 120.0), True)

    named_npcs = {
        "HBCT_Intro_Instructor": "Instructor",
        "HBCT_Motivation_Prasert": "Uncle Prasert",
        "HBCT_Motivation_Saeng": "Aunt Saeng",
        "HBCT_Motivation_Mali": "Aunt Mali",
        "HBCT_GoalSetting_AuntieNuan": "Auntie Nuan",
        "HBCT_GoalSetting_Nuan_ClearGoal": "Auntie Nuan",
        "HBCT_GoalSetting_UncleChai": "Uncle Chai",
        "HBCT_RoleModel_UncleSomchai": "Uncle Somchai",
        "HBCT_RoleModel_YoungAthlete": "Young Athlete",
    }
    for npc_label, display_name in named_npcs.items():
        configure_npc_nameplate(find_actor(npc_label), display_name)
    for generic_label in (
            "HBCT_GoalSetting_RoadVillager",
            "HBCT_RoleModel_RoadVillager"):
        configure_npc_nameplate(find_actor(generic_label), show_nameplate=False)

    for actor in editor_actors().get_all_level_actors():
        label = actor.get_actor_label()
        expected = protected_transforms.get(label)
        if expected is None:
            continue
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        scale = actor.get_actor_scale3d()
        actual = (
            location.x, location.y, location.z,
            rotation.pitch, rotation.yaw, rotation.roll,
            scale.x, scale.y, scale.z)
        if any(abs(before - after) > 0.01 for before, after in zip(expected, actual)):
            raise RuntimeError("Quest 3 authoring changed protected actor transform: {}".format(label))

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
        "HBCT_Location_VillageEntrance",
        "HBCT_Location_HealthPost",
        "HBCT_Location_Market",
        "HBCT_Trigger_PrasertObservation",
        "HBCT_Trigger_BadGoodDemo",
        "HBCT_Trigger_SaengAppreciation",
        "HBCT_Location_MaliHouse",
        "HBCT_GoalSetting_ObservationVHV",
        "HBCT_GoalSetting_AuntieNuan",
        "HBCT_GoalSetting_InitialObservation",
        "HBCT_GoalSetting_Nuan_ClearGoal",
        "HBCT_GoalSetting_UncleChai",
        "HBCT_GoalSetting_SelfSetVHV",
        "HBCT_GoalSetting_SelfSetPatient",
        "HBCT_GoalSetting_JointVHV",
        "HBCT_GoalSetting_JointPatient",
        "HBCT_GoalSetting_RoadVillager",
        "HBCT_Trigger_GoalSetting_InitialObservation",
        "HBCT_Trigger_GoalSetting_ClearGoal",
        "HBCT_Trigger_GoalSetting_NoGoal",
        "HBCT_Trigger_GoalSetting_SelfSet",
        "HBCT_Trigger_GoalSetting_Joint",
        "HBCT_Trigger_GoalSetting_MaliCheckIn",
        "HBCT_Trigger_GoalSetting_MaliPlanReview",
        "HBCT_Location_ExercisePark",
        "HBCT_RoleModel_ExerciseParkEntry",
        "HBCT_RoleModel_UncleSomchai",
        "HBCT_RoleModel_YoungAthlete",
        "HBCT_RoleModel_RoadVillager",
        "HBCT_Location_RoleModel_SaengCandidate",
        "HBCT_Location_RoleModel_Demo_Saeng",
        "HBCT_Location_RoleModel_Demo_Somchai",
        "HBCT_Location_RoleModel_WalkMid_Saeng",
        "HBCT_Location_RoleModel_WalkMid_Somchai",
        "HBCT_Location_RoleModel_WalkEnd_Saeng",
        "HBCT_Location_RoleModel_WalkEnd_Somchai",
        "HBCT_RoleModel_WalkAlongScene",
        "HBCT_Trigger_RoleModel_ExerciseObservation",
        "HBCT_Trigger_RoleModel_SimilarExperience",
        "HBCT_Trigger_RoleModel_Family",
        "HBCT_Trigger_RoleModel_Symbolic",
        "HBCT_Trigger_RoleModel_SymbolicCandidate",
        "HBCT_Trigger_RoleModel_TimelineBefore",
        "HBCT_Trigger_RoleModel_TimelineFindsModel",
        "HBCT_Trigger_RoleModel_TimelineFirstAttempt",
        "HBCT_Trigger_RoleModel_TimelineRepetition",
        "HBCT_Trigger_RoleModel_TimelineNewRoutine",
        "HBCT_Trigger_RoleModel_SaengDemo",
        "HBCT_NavLink_SaengHouseExit",
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
        "VHV.Participant.GoalSetting.RoadVillager",
        "VHV.Participant.UncleSomchai",
        "VHV.Participant.RoleModel.YoungAthlete",
        "VHV.Participant.RoleModel.RoadVillager",
    }
    if set(participant_tags) != expected_participants:
        raise RuntimeError("Unexpected production quest participant set: {}".format(participant_tags))
    if participant_tags.get("VHV.Participant.AuntSaeng") != "HBCT_Motivation_Saeng":
        raise RuntimeError("Quest 3 must reuse the single production Aunt Saeng actor")

    actors_by_label = {actor.get_actor_label(): actor for actor in actors}
    expected_nameplates = {
        "HBCT_Intro_Instructor": "Instructor",
        "HBCT_Motivation_Prasert": "Uncle Prasert",
        "HBCT_Motivation_Saeng": "Aunt Saeng",
        "HBCT_Motivation_Mali": "Aunt Mali",
        "HBCT_GoalSetting_AuntieNuan": "Auntie Nuan",
        "HBCT_GoalSetting_Nuan_ClearGoal": "Auntie Nuan",
        "HBCT_GoalSetting_UncleChai": "Uncle Chai",
        "HBCT_RoleModel_UncleSomchai": "Uncle Somchai",
        "HBCT_RoleModel_YoungAthlete": "Young Athlete",
    }
    for npc_label, display_name in expected_nameplates.items():
        nameplate = actors_by_label[npc_label].get_nameplate_component()
        if (not nameplate.get_editor_property("show_nameplate")
                or str(nameplate.get_editor_property("nameplate_display_name")) != display_name):
            raise RuntimeError("Unexpected NPC nameplate for {}".format(npc_label))
    for generic_label in ("HBCT_GoalSetting_RoadVillager", "HBCT_RoleModel_RoadVillager"):
        if actors_by_label[generic_label].get_nameplate_component().get_editor_property("show_nameplate"):
            raise RuntimeError("Generic roadside NPC must not show a nameplate: {}".format(generic_label))
    instructor_actors = [actor for actor in actors
                         if isinstance(actor, unreal.VHVNPCCharacter)
                         and actor.get_actor_label() == "HBCT_Intro_Instructor"]
    if len(instructor_actors) != 1:
        raise RuntimeError("Production map must contain exactly one HBCT Instructor actor")
    instructor_interaction = instructor_actors[0].get_npc_interaction_component()
    if (instructor_interaction.get_editor_property("use_quest_objective_text_as_prompt")
            or str(instructor_interaction.get_editor_property("default_interaction_prompt")) != "Talk"
            or not instructor_interaction.get_editor_property("interaction_enabled")):
        raise RuntimeError("Instructor must use the normal Talk interaction prompt")
    if instructor_actors[0].get_dialogue_component().get_editor_property("default_conversation"):
        raise RuntimeError("Instructor must not offer fallback dialogue outside an active objective")

    semantic_locations = {
        "HBCT_Location_VillageEntrance": "VHV.Location.HBCT.VillageEntrance",
        "HBCT_Location_HealthPost": "VHV.Location.HBCT.HealthPost",
        "HBCT_Location_ExercisePark": "VHV.Location.HBCT.ExercisePark",
    }
    for label, expected_tag in semantic_locations.items():
        matches = [actor for actor in actors
                   if isinstance(actor, unreal.VHVQuestLocationVolume)
                   and str(unreal.GameplayTagLibrary.get_tag_name(
                       actor.get_editor_property("location_tag"))) == expected_tag]
        if len(matches) != 1 or matches[0].get_actor_label() != label:
            raise RuntimeError("Semantic Instructor destination {} must have exactly one location actor".format(expected_tag))
        location_actor = matches[0]
        if (location_actor.get_editor_property("enabled")
                or location_actor.get_editor_property("trigger_world_action")
                or location_actor.get_editor_property("activate_current_objective")):
            raise RuntimeError("Semantic Instructor destination {} must not react to player entry".format(label))
    saeng_candidate_location = actors_by_label["HBCT_Location_RoleModel_SaengCandidate"]
    if (str(unreal.GameplayTagLibrary.get_tag_name(
            saeng_candidate_location.get_editor_property("location_tag")))
            != "VHV.Location.HBCT.RoleModel.SaengCandidate"):
        raise RuntimeError("Aunt Saeng candidate destination has the wrong semantic location tag")
    if (saeng_candidate_location.get_editor_property("enabled")
            or saeng_candidate_location.get_editor_property("trigger_world_action")
            or saeng_candidate_location.get_editor_property("activate_current_objective")):
        raise RuntimeError("Aunt Saeng candidate destination must remain an NPC-only movement point")
    role_move_locations = {
        "HBCT_Location_RoleModel_SaengCandidate": "VHV.Location.HBCT.RoleModel.SaengCandidate",
        "HBCT_Location_RoleModel_Demo_Saeng": "VHV.Location.HBCT.RoleModel.SaengDemo",
        "HBCT_Location_RoleModel_Demo_Somchai": "VHV.Location.HBCT.RoleModel.SomchaiDemo",
        "HBCT_Location_RoleModel_WalkMid_Saeng": "VHV.Location.HBCT.RoleModel.SaengWalkMid",
        "HBCT_Location_RoleModel_WalkMid_Somchai": "VHV.Location.HBCT.RoleModel.SomchaiWalkMid",
        "HBCT_Location_RoleModel_WalkEnd_Saeng": "VHV.Location.HBCT.RoleModel.SaengWalkEnd",
        "HBCT_Location_RoleModel_WalkEnd_Somchai": "VHV.Location.HBCT.RoleModel.SomchaiWalkEnd",
    }
    expected_move_leaves = {location_tag.rsplit(".", 1)[-1]
                            for location_tag in role_move_locations.values()}
    if len(expected_move_leaves) != len(role_move_locations):
        raise RuntimeError("Quest 3 movement destinations must have unique semantic leaf IDs")
    for label, expected_tag in role_move_locations.items():
        label_matches = [actor for actor in actors if actor.get_actor_label() == label]
        tag_matches = [actor for actor in actors
                       if isinstance(actor, unreal.VHVQuestLocationVolume)
                       and str(unreal.GameplayTagLibrary.get_tag_name(
                           actor.get_editor_property("location_tag"))) == expected_tag]
        if len(label_matches) != 1 or len(tag_matches) != 1 or tag_matches[0] != label_matches[0]:
            raise RuntimeError(
                "Quest 3 movement destination {} must resolve only to {}".format(expected_tag, label))
    for actor in actors:
        if not isinstance(actor, unreal.VHVQuestLocationVolume):
            continue
        authored_tag = str(unreal.GameplayTagLibrary.get_tag_name(
            actor.get_editor_property("location_tag")))
        authored_leaf = authored_tag.rsplit(".", 1)[-1]
        if authored_leaf in ("Saeng", "Somchai"):
            raise RuntimeError("Ambiguous Quest 3 movement destination leaf remains on {}: {}".format(
                actor.get_actor_label(), authored_tag))
    saeng_actor = actors_by_label["HBCT_Motivation_Saeng"]
    saeng_interaction = saeng_actor.get_npc_interaction_component()
    if (str(saeng_interaction.get_editor_property("default_interaction_prompt")) != "Talk"
            or not saeng_interaction.get_editor_property("interaction_enabled")):
        raise RuntimeError("Aunt Saeng candidate interaction must use normal Talk/E")
    saeng_default_conversation = str(
        saeng_actor.get_dialogue_component().get_editor_property("default_conversation"))
    if "DA_Conversation_HBCT_RoleModel_SaengCandidate" not in saeng_default_conversation:
        raise RuntimeError("Aunt Saeng is not routed to the Quest 3 candidate conversation")
    road_interaction = actors_by_label["HBCT_GoalSetting_RoadVillager"].get_npc_interaction_component()
    if (road_interaction.get_editor_property("use_quest_objective_text_as_prompt")
            or str(road_interaction.get_editor_property("default_interaction_prompt")) != "Talk"
            or not road_interaction.get_editor_property("interaction_enabled")):
        raise RuntimeError("Road villager must use the normal Talk interaction prompt")
    if actors_by_label["HBCT_GoalSetting_RoadVillager"].get_dialogue_component().get_editor_property(
            "default_conversation"):
        raise RuntimeError("Road villager must not offer fallback dialogue outside its quest objective")
    road_location = actors_by_label["HBCT_GoalSetting_RoadVillager"].get_actor_location()
    mali_location = actors_by_label["HBCT_Motivation_Mali"].get_actor_location()
    instructor_location = actors_by_label["HBCT_Intro_Instructor"].get_actor_location()
    road_from_mali_squared = ((road_location.x - mali_location.x) ** 2
                              + (road_location.y - mali_location.y) ** 2)
    road_to_instructor_squared = ((road_location.x - instructor_location.x) ** 2
                                  + (road_location.y - instructor_location.y) ** 2)
    mali_to_instructor_squared = ((mali_location.x - instructor_location.x) ** 2
                                  + (mali_location.y - instructor_location.y) ** 2)
    if road_from_mali_squared < 500.0 ** 2 or road_to_instructor_squared >= mali_to_instructor_squared:
        raise RuntimeError("Road villager must be outside Mali's area and farther along toward the Instructor")

    expected_goal_triggers = {
        "HBCT_Trigger_GoalSetting_InitialObservation": ("O02_ObserveGoalConversation", False, "VHV.WorldReceiver.HBCT.GoalSetting.InitialObservation", ["HBCT_GoalSetting_ObservationVHV", "HBCT_GoalSetting_AuntieNuan"]),
        "HBCT_Trigger_GoalSetting_ClearGoal": ("O05_ComparePatients", False, "VHV.WorldReceiver.HBCT.GoalSetting.ClearGoal", ["HBCT_GoalSetting_Nuan_ClearGoal"]),
        "HBCT_Trigger_GoalSetting_NoGoal": ("O05_ComparePatients", False, "VHV.WorldReceiver.HBCT.GoalSetting.NoGoal", ["HBCT_GoalSetting_UncleChai"]),
        "HBCT_Trigger_GoalSetting_SelfSet": ("O07_ObserveGoalMethods", False, "VHV.WorldReceiver.HBCT.GoalSetting.SelfSet", ["HBCT_GoalSetting_SelfSetVHV", "HBCT_GoalSetting_SelfSetPatient"]),
        "HBCT_Trigger_GoalSetting_Joint": ("O07_ObserveGoalMethods", False, "VHV.WorldReceiver.HBCT.GoalSetting.Joint", ["HBCT_GoalSetting_JointVHV", "HBCT_GoalSetting_JointPatient"]),
        "HBCT_Trigger_GoalSetting_MaliCheckIn": ("O10_ReturnToMali", True, "", ["HBCT_Motivation_Mali"]),
        "HBCT_Trigger_GoalSetting_MaliPlanReview": ("O15_ReviewGoalPlan", True, "", ["HBCT_Motivation_Mali"]),
    }
    actual_goal_trigger_labels = {
        actor.get_actor_label() for actor in actors
        if actor.get_actor_label().startswith("HBCT_Trigger_GoalSetting_")}
    if actual_goal_trigger_labels != set(expected_goal_triggers):
        raise RuntimeError("Unexpected/orphaned Quest 2 triggers: expected {}, found {}".format(
            sorted(expected_goal_triggers), sorted(actual_goal_trigger_labels)))
    for trigger_label, (objective_id, activates_objective, receiver_tag, group_labels) in expected_goal_triggers.items():
        trigger = actors_by_label[trigger_label]
        if (str(trigger.get_editor_property("required_active_quest_id")) != "Q_HBCT_02_GOAL_SETTING"
                or str(trigger.get_editor_property("required_active_objective_id")) != objective_id
                or trigger.get_editor_property("activate_current_objective") != activates_objective):
            raise RuntimeError("Quest 2 trigger {} is not gated/routed to {}".format(
                trigger_label, objective_id))
        if receiver_tag:
            actual_receiver = str(unreal.GameplayTagLibrary.get_tag_name(
                trigger.get_editor_property("world_action_receiver_tag")))
            if (not trigger.get_editor_property("trigger_world_action")
                    or actual_receiver != receiver_tag):
                raise RuntimeError("Quest 2 trigger {} has invalid WorldAction routing".format(
                    trigger_label))
        trigger_location = trigger.get_actor_location()
        group_locations = [actors_by_label[label].get_actor_location() for label in group_labels]
        center = unreal.Vector(
            sum(value.x for value in group_locations) / len(group_locations),
            sum(value.y for value in group_locations) / len(group_locations),
            sum(value.z for value in group_locations) / len(group_locations))
        distance_squared = ((trigger_location.x - center.x) ** 2
                            + (trigger_location.y - center.y) ** 2
                            + (trigger_location.z - center.z) ** 2)
        if distance_squared > 900.0 ** 2:
            raise RuntimeError("Quest 2 trigger {} is disconnected from {}".format(
                trigger_label, group_labels))

    expected_role_triggers = {
        "HBCT_Trigger_RoleModel_ExerciseObservation": ("O02_DiscoverRoleModels", False, "VHV.WorldReceiver.HBCT.RoleModel.ExerciseObservation", ["HBCT_RoleModel_FitVillager", "HBCT_RoleModel_YoungObserver"]),
        "HBCT_Trigger_RoleModel_SimilarExperience": ("O02_DiscoverRoleModels", False, "VHV.WorldReceiver.HBCT.RoleModel.SimilarExperience", ["HBCT_RoleModel_ExperienceWoman", "HBCT_RoleModel_ListeningWoman"]),
        "HBCT_Trigger_RoleModel_Family": ("O02_DiscoverRoleModels", False, "VHV.WorldReceiver.HBCT.RoleModel.Family", ["HBCT_RoleModel_Parent", "HBCT_RoleModel_YoungerFamilyMember"]),
        "HBCT_Trigger_RoleModel_Symbolic": ("O02_DiscoverRoleModels", False, "VHV.WorldReceiver.HBCT.RoleModel.Symbolic", ["HBCT_RoleModel_SymbolicObserver", "HBCT_RoleModel_SymbolicPoster"]),
        "HBCT_Trigger_RoleModel_SymbolicCandidate": ("O06_InspectCandidates", False, "VHV.WorldReceiver.HBCT.RoleModel.SymbolicCandidate", ["HBCT_RoleModel_SymbolicCandidateDisplay", "HBCT_RoleModel_SymbolicCandidateObserver"]),
        "HBCT_Trigger_RoleModel_TimelineBefore": ("O09_SeeChangeOverTime", False, "VHV.WorldReceiver.HBCT.RoleModel.Timeline.Before", ["HBCT_RoleModel_TimelineBefore_Target"]),
        "HBCT_Trigger_RoleModel_TimelineFindsModel": ("O09_SeeChangeOverTime", False, "VHV.WorldReceiver.HBCT.RoleModel.Timeline.FindsModel", ["HBCT_RoleModel_TimelineFindsModel_Target", "HBCT_RoleModel_TimelineFindsModel_Model"]),
        "HBCT_Trigger_RoleModel_TimelineFirstAttempt": ("O09_SeeChangeOverTime", False, "VHV.WorldReceiver.HBCT.RoleModel.Timeline.FirstAttempt", ["HBCT_RoleModel_TimelineFirstAttempt_Target"]),
        "HBCT_Trigger_RoleModel_TimelineRepetition": ("O09_SeeChangeOverTime", False, "VHV.WorldReceiver.HBCT.RoleModel.Timeline.Repetition", ["HBCT_RoleModel_TimelineRepetition_Target", "HBCT_RoleModel_TimelineRepetition_Model"]),
        "HBCT_Trigger_RoleModel_TimelineNewRoutine": ("O09_SeeChangeOverTime", False, "VHV.WorldReceiver.HBCT.RoleModel.Timeline.NewRoutine", ["HBCT_RoleModel_TimelineNewRoutine_Target"]),
        "HBCT_Trigger_RoleModel_SaengDemo": ("O12_ObserveSaeng", True, "", ["HBCT_Location_RoleModel_Demo_Saeng", "HBCT_Location_RoleModel_Demo_Somchai"]),
    }
    actual_role_trigger_labels = {
        actor.get_actor_label() for actor in actors
        if actor.get_actor_label().startswith("HBCT_Trigger_RoleModel_")}
    if actual_role_trigger_labels != set(expected_role_triggers):
        raise RuntimeError("Unexpected/orphaned Quest 3 triggers: expected {}, found {}".format(
            sorted(expected_role_triggers), sorted(actual_role_trigger_labels)))
    for trigger_label, (objective_id, activates_objective, receiver_tag, group_labels) in expected_role_triggers.items():
        trigger = actors_by_label[trigger_label]
        if (str(trigger.get_editor_property("required_active_quest_id")) != "Q_HBCT_03_ROLE_MODEL"
                or str(trigger.get_editor_property("required_active_objective_id")) != objective_id
                or trigger.get_editor_property("activate_current_objective") != activates_objective):
            raise RuntimeError("Quest 3 trigger {} is not gated/routed to {}".format(
                trigger_label, objective_id))
        if receiver_tag:
            actual_receiver = str(unreal.GameplayTagLibrary.get_tag_name(
                trigger.get_editor_property("world_action_receiver_tag")))
            if (not trigger.get_editor_property("trigger_world_action")
                    or actual_receiver != receiver_tag
                    or trigger.get_editor_property("track_world_action_as_objective")):
                raise RuntimeError("Quest 3 ambient trigger {} has invalid WorldAction routing".format(
                    trigger_label))
        trigger_location = trigger.get_actor_location()
        group_locations = [actors_by_label[label].get_actor_location() for label in group_labels]
        center = unreal.Vector(
            sum(value.x for value in group_locations) / len(group_locations),
            sum(value.y for value in group_locations) / len(group_locations),
            sum(value.z for value in group_locations) / len(group_locations))
        xy_offset_squared = ((trigger_location.x - center.x) ** 2
                             + (trigger_location.y - center.y) ** 2)
        if xy_offset_squared > 1.0:
            raise RuntimeError("Quest 3 trigger {} is not centered on {} in XY".format(
                trigger_label, group_labels))

    spaced_role_triggers = [actors_by_label["HBCT_RoleModel_ExerciseParkEntry"]]
    spaced_role_triggers.extend(actors_by_label[label] for label in expected_role_triggers)
    for index, first in enumerate(spaced_role_triggers):
        first_location = first.get_actor_location()
        first_extent = first.get_editor_property("box_component").get_unscaled_box_extent()
        for second in spaced_role_triggers[index + 1:]:
            second_location = second.get_actor_location()
            second_extent = second.get_editor_property("box_component").get_unscaled_box_extent()
            clear_x = max(0.0, abs(first_location.x - second_location.x)
                          - first_extent.x - second_extent.x)
            clear_y = max(0.0, abs(first_location.y - second_location.y)
                          - first_extent.y - second_extent.y)
            edge_clearance = (clear_x ** 2 + clear_y ** 2) ** 0.5
            if edge_clearance < 499.0:
                raise RuntimeError(
                    "Quest 3 triggers {} and {} have only {:.1f} cm edge clearance".format(
                        first.get_actor_label(), second.get_actor_label(), edge_clearance))

    if any("Instructor" in label for label in actual_role_trigger_labels):
        raise RuntimeError("Quest 3 must not create an Instructor dialogue trigger")
    role_npcs = [actor for actor in actors
                 if isinstance(actor, unreal.VHVNPCCharacter)
                 and actor.get_actor_label().startswith("HBCT_RoleModel_")]
    for actor in role_npcs:
        location = actor.get_actor_location()
        if (abs(location.z - 100.0) > 5.0 or location.x < -1000.0 or location.x > 5300.0
                or location.y < 2900.0 or location.y > 5400.0):
            raise RuntimeError("New Quest 3 NPC {} is off the accessible staging floor: {}".format(
                actor.get_actor_label(), location))
    for index, first in enumerate(role_npcs):
        first_location = first.get_actor_location()
        for second in role_npcs[index + 1:]:
            second_location = second.get_actor_location()
            distance_squared = ((first_location.x - second_location.x) ** 2
                                + (first_location.y - second_location.y) ** 2)
            if distance_squared < 90.0 ** 2:
                raise RuntimeError("Quest 3 NPC capsules overlap: {} and {}".format(
                    first.get_actor_label(), second.get_actor_label()))

    movement_waypoint_labels = (
            "HBCT_Location_RoleModel_SaengCandidate",
            "HBCT_Location_RoleModel_Demo_Saeng",
            "HBCT_Location_RoleModel_Demo_Somchai",
            "HBCT_Location_RoleModel_WalkMid_Saeng",
            "HBCT_Location_RoleModel_WalkMid_Somchai",
            "HBCT_Location_RoleModel_WalkEnd_Saeng",
            "HBCT_Location_RoleModel_WalkEnd_Somchai")
    for waypoint_label in movement_waypoint_labels:
        location = actors_by_label[waypoint_label].get_actor_location()
        if (abs(location.z - 100.0) > 1.0 or location.x < -1000.0 or location.x > 5300.0
                or location.y < 2900.0 or location.y > 5400.0):
            raise RuntimeError("Quest 3 movement point is off the staged walkable floor: {}".format(
                waypoint_label))

    nav_bounds = [actor for actor in actors if isinstance(actor, unreal.NavMeshBoundsVolume)]
    if not nav_bounds:
        raise RuntimeError("Production map has no NavMesh bounds for Quest 3 movement")
    nav_boxes = [volume.get_actor_bounds(False, False) for volume in nav_bounds]
    for waypoint_label in movement_waypoint_labels:
        location = actors_by_label[waypoint_label].get_actor_location()
        if not any(
                abs(location.x - origin.x) <= extent.x
                and abs(location.y - origin.y) <= extent.y
                and abs(location.z - origin.z) <= extent.z
                for origin, extent in nav_boxes):
            raise RuntimeError("Quest 3 movement point is outside NavMesh bounds: {}".format(
                waypoint_label))

    for ambient_actor in actors:
        if not (isinstance(ambient_actor, unreal.VHVAmbientConversationActor)
                and ambient_actor.get_actor_label().startswith("HBCT_RoleModel_")):
            continue
        if ambient_actor.get_actor_label() == "HBCT_RoleModel_WalkAlongScene":
            receiver = ambient_actor.get_editor_property("world_action_receiver")
            receiver_tag = str(unreal.GameplayTagLibrary.get_tag_name(
                receiver.get_editor_property("receiver_tag")))
            if (ambient_actor.get_editor_property("auto_start_on_begin_play")
                    or ambient_actor.get_editor_property("requires_explicit_trigger")
                    or not ambient_actor.get_editor_property("play_once")
                    or not ambient_actor.get_editor_property("preserve_participant_behavior")
                    or receiver_tag != "VHV.WorldReceiver.HBCT.RoleModel.WalkAlong"):
                raise RuntimeError("Quest 3 walk-along speech must be quest-started, non-modal, and play once")
            bound_slots = {
                str(binding.get_editor_property("slot_id")): binding.get_editor_property("npc").get_actor_label()
                for binding in ambient_actor.get_editor_property("participants")}
            if bound_slots != {
                    "Saeng": "HBCT_Motivation_Saeng",
                    "Somchai": "HBCT_RoleModel_UncleSomchai"}:
                raise RuntimeError("Quest 3 walk-along speech has incorrect persistent NPC bindings")
        elif (ambient_actor.get_editor_property("auto_start_on_begin_play")
                or not ambient_actor.get_editor_property("requires_explicit_trigger")
                or not ambient_actor.get_editor_property("play_once")):
            raise RuntimeError("Quest 3 ambient scene {} must be explicit and play once".format(
                ambient_actor.get_actor_label()))

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
    authored_activities = [activity_data for topic in topics
                           for activity_data in topic.get_editor_property("activities")]
    activity_ids = {
        str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property("activity_tag"))).split(".")[-1]
        for item in authored_activities
    }
    if len(authored_activities) != 40 or len(activity_ids) != 40:
        raise RuntimeError("HBCT production content must contain 40 uniquely addressed activities")

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

        if (item.get_editor_property("activity_type") == unreal.TextbookActivityType.SINGLE_CHOICE
                and not str(unreal.GameplayTagLibrary.get_tag_name(
                    item.get_editor_property("activity_tag"))).startswith("VHV.Activity.HBCT.RoleModel.")):
            policy = item.get_editor_property("attempt_policy")
            if (not policy.get_editor_property("enabled")
                    or policy.get_editor_property("max_attempts") != 2
                    or not str(policy.get_editor_property("first_incorrect_hint"))
                    or not str(policy.get_editor_property("final_incorrect_explanation"))):
                raise RuntimeError("Assessed HBCT choice {} lacks the two-attempt policy".format(activity_id))

    def has_meaningful_teaching(teaching_data):
        return bool(
            str(teaching_data.get_editor_property("title")).strip()
            or str(teaching_data.get_editor_property("content")).strip()
            or any(str(value).strip() for value in teaching_data.get_editor_property("key_takeaways"))
            or teaching_data.get_editor_property("media")
            or teaching_data.get_editor_property("media_texture")
            or str(teaching_data.get_editor_property("media_caption")).strip())

    role_activity_ids = {
        activity_id for activity_id in activities_by_id
        if str(unreal.GameplayTagLibrary.get_tag_name(
            activities_by_id[activity_id].get_editor_property("activity_tag"))).startswith(
                "VHV.Activity.HBCT.RoleModel.")}
    if len(role_activity_ids) != 14:
        raise RuntimeError("Quest 3 must contain 14 uniquely addressed activities")
    if any(has_meaningful_teaching(activities_by_id[activity_id].get_editor_property("teaching"))
           for activity_id in role_activity_ids):
        raise RuntimeError("Quest 3 must not author teaching pages")
    select_policy = activities_by_id["SelectModel"].get_editor_property("attempt_policy")
    if (not select_policy.get_editor_property("enabled")
            or select_policy.get_editor_property("max_attempts") != 2):
        raise RuntimeError("Quest 3 role-model selection must use the two-attempt policy")

    intentional_goal_lessons = {"WhatIsGoalSetting", "UsefulGoalCharacteristics"}
    goal_activity_ids = {
        activity_id for activity_id in activities_by_id
        if activity_id in {
            "GoalParts", "WhatIsGoalSetting", "ClearerPath", "GoalFunction",
            "IdentifyMethods", "UsefulGoals", "DiagnoseVagueGoal", "DiagnoseExtremeGoal",
            "UsefulGoalCharacteristics", "ImproveVagueGoal", "MaliDifficulty",
            "MaliBehaviors", "MaliGoalBoardA", "MaliGoalBoardB", "FinalTransfer"}}
    authored_goal_lessons = {
        activity_id for activity_id in goal_activity_ids
        if has_meaningful_teaching(activities_by_id[activity_id].get_editor_property("teaching"))}
    if authored_goal_lessons != intentional_goal_lessons:
        raise RuntimeError("Quest 2 teaching pages must be exactly {} but found {}".format(
            sorted(intentional_goal_lessons), sorted(authored_goal_lessons)))

    expected_lessons = {
        "WhatIsGoalSetting": (
            "WHAT IS GOAL SETTING?",
            "Goal setting means deciding what health behavior should change and defining a clear target to work toward.",
            [
                "BEHAVIOR — What will change?",
                "TARGET — What are we trying to achieve?",
                "PATH — What practical action moves us toward it?",
                "Motivation gives energy. Goals give direction.",
            ]),
        "UsefulGoalCharacteristics": (
            "WHAT MAKES A USEFUL GOAL?",
            "",
            [
                "SPECIFIC — The behavior is clear.",
                "CHALLENGING — The goal requires meaningful effort.",
                "CLEARLY DIRECTED — The person knows what they are working toward.",
                "SHORT-TERM — There is an achievable target that can be worked on now.",
                "REALISTIC — The goal is possible for this person and situation.",
                "A useful goal should guide action, not merely sound ambitious.",
            ]),
    }
    for activity_id, (expected_title, expected_body, expected_cards) in expected_lessons.items():
        lesson_data = activities_by_id[activity_id].get_editor_property("teaching")
        actual_cards = [str(value) for value in lesson_data.get_editor_property("key_takeaways")]
        if (str(lesson_data.get_editor_property("title")) != expected_title
                or str(lesson_data.get_editor_property("content")) != expected_body
                or actual_cards != expected_cards):
            raise RuntimeError("Quest 2 lesson {} is incomplete or split incorrectly".format(activity_id))

    expected_classifications = {
        "IdentifyMethods": (
            "Which examples show a SELF-SET goal?",
            [True, False, True, False]),
        "MaliGoalBoardA": (
            "Which statements are SHORT-TERM GOALS in Mali's plan?",
            [True, True, False, False]),
        "MaliGoalBoardB": (
            "Which statements describe Mali's LONG-TERM GOALS?",
            [True, True, False]),
    }
    for activity_id, (expected_question, expected_answers) in expected_classifications.items():
        activity_data = activities_by_id[activity_id]
        question_data = activity_data.get_editor_property("question")
        actual_answers = [option.get_editor_property("is_correct")
                          for option in question_data.get_editor_property("options")]
        if (activity_data.get_editor_property("activity_type")
                != unreal.TextbookActivityType.MULTI_CHOICE
                or activity_data.get_editor_property("matching_pairs")
                or str(question_data.get_editor_property("question_text")) != expected_question
                or actual_answers != expected_answers):
            raise RuntimeError("Quest 2 classification {} is not authored as clear MultiChoice".format(
                activity_id))

    remaining_goal_matching = [
        activity_id for activity_id in goal_activity_ids
        if activities_by_id[activity_id].get_editor_property("activity_type")
        == unreal.TextbookActivityType.MATCHING]
    if remaining_goal_matching:
        raise RuntimeError("Quest 2 still contains Matching activities: {}".format(
            remaining_goal_matching))

    expected_categories = {
        "IntroLesson": unreal.TextbookTeachingCategory.LESSON,
        "TechniqueTitle": unreal.TextbookTeachingCategory.TECHNIQUE,
        "MotivationLesson": unreal.TextbookTeachingCategory.KEY_IDEA,
        "DemoReflection": unreal.TextbookTeachingCategory.REFLECTION,
        "Methods": unreal.TextbookTeachingCategory.LESSON,
        "WhatIsGoalSetting": unreal.TextbookTeachingCategory.LESSON,
        "UsefulGoalCharacteristics": unreal.TextbookTeachingCategory.LESSON,
    }
    for activity_id, expected_category in expected_categories.items():
        actual = activities_by_id[activity_id].get_editor_property("teaching").get_editor_property("category")
        if actual != expected_category:
            raise RuntimeError("{} has teaching category {}; expected {}".format(
                activity_id, actual, expected_category))

    technique_stinger = activities_by_id["TechniqueTitle"].get_editor_property("major_stinger")
    if (str(technique_stinger.get_editor_property("label")) != "TECHNIQUE 1"
            or str(technique_stinger.get_editor_property("title")) != "BUILDING MOTIVATION"
            or str(technique_stinger.get_editor_property("subtitle")) != "HEALTH BEHAVIOR CHANGE"
            or abs(technique_stinger.get_editor_property("hold_duration") - 1.8) > 0.001):
        raise RuntimeError("TechniqueTitle major stinger is not authored correctly")

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

    mali_review_nodes = conversations["DA_Conversation_HBCT_GoalSetting_MaliReview"].get_editor_property(
        "conversation").get_editor_property("nodes")
    if ([str(node.get_editor_property("node_id")) for node in mali_review_nodes]
            != ["GSMaliReview_01", "GSMaliReview_02", "GSMaliReview_03", "GSMaliReview_04"]
            or any(node.get_editor_property("node_type")
                   == unreal.VHVDialogueNodeType.LEARNING_ACTIVITY for node in mali_review_nodes)):
        raise RuntimeError("Mali review must end in gameplay without launching FinalTransfer")

    roadside_nodes = conversations["DA_Conversation_HBCT_GoalSetting_RoadsidePractice"].get_editor_property(
        "conversation").get_editor_property("nodes")
    if [str(node.get_editor_property("node_id")) for node in roadside_nodes] != [
            "GSRoad_01", "GSRoad_02", "GSRoad_03", "GSRoad_04",
            "GSRoad_05", "GSRoad_06", "GSRoad_07", "GSRoad_08"]:
        raise RuntimeError("Roadside practice dialogue sequence is incomplete")
    transfer_links = []
    for asset_name, asset in conversations.items():
        nodes = asset.get_editor_property("conversation").get_editor_property("nodes")
        for node in nodes:
            if node.get_editor_property("node_type") != unreal.VHVDialogueNodeType.LEARNING_ACTIVITY:
                continue
            linked = node.get_editor_property("linked_activity").get_editor_property("activity_tag")
            if str(unreal.GameplayTagLibrary.get_tag_name(linked)).endswith(".FinalTransfer"):
                transfer_links.append((asset_name, str(node.get_editor_property("node_id"))))
    if transfer_links != [("DA_Conversation_HBCT_GoalSetting_RoadsidePractice", "GSRoad_05")]:
        raise RuntimeError("FinalTransfer must be linked exactly once from the roadside dialogue: {}".format(
            transfer_links))

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
        "Q_HBCT_02_GOAL_SETTING": [
            "O01_ReachHealthPost", "O02_ObserveGoalConversation", "O03_IdentifyGoalParts",
            "O04_LearnGoalSetting", "O05_ComparePatients", "O06_ReflectOnDirection",
            "O07_ObserveGoalMethods", "O08_IdentifyGoalMethods", "O09_EvaluateGoals",
            "O10_ReturnToMali", "O11_FindCurrentDifficulty", "O12_DefineDesiredBehavior",
            "O13_BuildShortTermGoal", "O14_LinkLongTermGoal", "O15_ReviewGoalPlan",
            "O15B_RoadsidePractice", "O16_FinalDebrief"],
        "Q_HBCT_03_ROLE_MODEL": [
            "O01_ReachExercisePark", "O02_DiscoverRoleModels", "O03_ClassifyModels",
            "O04_MeetSomchai", "O05_UnderstandSomchai", "O06_InspectCandidates",
            "O07_SelectRoleModel", "O08_ExplainModelFit", "O09_SeeChangeOverTime",
            "O10_ReconstructModeling", "O11_PrepareDemonstration", "O12_ObserveSaeng",
            "O13_SupportImitation", "O14_PracticeTogether", "O15_FinalApplication",
            "O16_FinalDebrief"],
    }
    if len(quests) != 4:
        raise RuntimeError("HBCT production arc must contain exactly four quests through Technique 3")
    for quest in quests:
        quest_id = str(quest.get_editor_property("quest_id"))
        objective_ids = [str(item.get_editor_property("objective_id"))
                         for item in quest.get_editor_property("objectives")]
        if objective_ids != expected.get(quest_id):
            raise RuntimeError("Unexpected objective sequence for {}: {}".format(quest_id, objective_ids))

    motivation_quest = next(quest for quest in quests
                            if str(quest.get_editor_property("quest_id")) == "Q_HBCT_01_MOTIVATION")
    explicit_objectives = {
        "O02_ObservePrasert",
        "O04_ObserveBadGoodDemo",
        "O07_ObserveAppreciation",
    }
    for item in motivation_quest.get_editor_property("objectives"):
        objective_id = str(item.get_editor_property("objective_id"))
        if (objective_id in explicit_objectives
                and item.get_editor_property("world_action_start_policy")
                != unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER):
            raise RuntimeError("Production WorldAction objective {} must use Explicit Trigger".format(
                objective_id))
    completion_stinger = motivation_quest.get_editor_property("completion_stinger")
    if (str(completion_stinger.get_editor_property("label")) != "QUEST COMPLETE"
            or str(completion_stinger.get_editor_property("title")) != "BUILDING MOTIVATION"
            or str(completion_stinger.get_editor_property("subtitle")) != "TECHNIQUE 1"
            or not motivation_quest.get_editor_property("auto_start_next_quest")):
        raise RuntimeError("Building Motivation completion transition is not authored correctly")
    initial_moves = arc.get_editor_property("initial_npc_moves")
    if len(initial_moves) != 1:
        raise RuntimeError("HBCT arc must author one initial persistent NPC destination")
    initial_participant = str(unreal.GameplayTagLibrary.get_tag_name(
        initial_moves[0].get_editor_property("participant_tag")))
    initial_destination = str(unreal.GameplayTagLibrary.get_tag_name(
        initial_moves[0].get_editor_property("destination_location_tag")))
    if (initial_participant != "VHV.Participant.Instructor"
            or initial_destination != "VHV.Location.HBCT.VillageEntrance"):
        raise RuntimeError("Instructor initial destination must be VillageEntrance")
    motivation_objectives = {str(item.get_editor_property("objective_id")): item
                             for item in motivation_quest.get_editor_property("objectives")}
    departure_moves = motivation_objectives["O14_FinalDebrief"].get_editor_property(
        "completion_npc_moves")
    if len(departure_moves) != 1:
        raise RuntimeError("Motivation final debrief must issue one Instructor departure move")
    departure_participant = str(unreal.GameplayTagLibrary.get_tag_name(
        departure_moves[0].get_editor_property("participant_tag")))
    departure_destination = str(unreal.GameplayTagLibrary.get_tag_name(
        departure_moves[0].get_editor_property("destination_location_tag")))
    if (departure_participant != "VHV.Participant.Instructor"
            or departure_destination != "VHV.Location.HBCT.HealthPost"):
        raise RuntimeError("Motivation final debrief must move Instructor to HealthPost")

    goal_setting_quest = next(quest for quest in quests
                              if str(quest.get_editor_property("quest_id")) == "Q_HBCT_02_GOAL_SETTING")
    start_stinger = goal_setting_quest.get_editor_property("start_stinger")
    if (str(start_stinger.get_editor_property("label")) != "TECHNIQUE 2"
            or str(start_stinger.get_editor_property("title")) != "GOAL SETTING"
            or str(start_stinger.get_editor_property("subtitle")) != "HEALTH BEHAVIOR CHANGE"):
        raise RuntimeError("Goal Setting start transition is not authored correctly")
    completion_stinger = goal_setting_quest.get_editor_property("completion_stinger")
    if (str(completion_stinger.get_editor_property("label")) != "QUEST COMPLETE"
            or str(completion_stinger.get_editor_property("title")) != "GOAL SETTING"
            or str(completion_stinger.get_editor_property("subtitle")) != "TECHNIQUE 2"):
        raise RuntimeError("Goal Setting completion transition is not authored correctly")

    goal_objectives = {str(item.get_editor_property("objective_id")): item
                       for item in goal_setting_quest.get_editor_property("objectives")}
    roadside_objective = goal_objectives["O15B_RoadsidePractice"]
    roadside_participant = str(unreal.GameplayTagLibrary.get_tag_name(
        roadside_objective.get_editor_property("participant_tag")))
    if (roadside_objective.get_editor_property("objective_type")
            != unreal.VHVQuestObjectiveType.CONVERSATION
            or roadside_participant != "VHV.Participant.GoalSetting.RoadVillager"
            or str(roadside_objective.get_editor_property("entry_node_id")) != "GSRoad_01"
            or roadside_objective.get_editor_property("auto_start")):
        raise RuntimeError("Roadside practice objective must wait for interaction with its villager")
    expected_goal_tracker_text = {
        "O01_ReachHealthPost": "Meet the Instructor at the community health post",
        "O02_ObserveGoalConversation": "Approach the nearby VHV and observe how an intention becomes a goal",
        "O04_LearnGoalSetting": "Talk to the Instructor about what goal setting means",
        "O05_ComparePatients": "Visit both nearby villagers and compare a person with a goal and one without",
        "O06_ReflectOnDirection": "Talk to the Instructor about why goals matter",
        "O07_ObserveGoalMethods": "Visit both demonstration areas and observe two ways of setting goals",
        "O08_IdentifyGoalMethods": "Talk to the Instructor about the two goal-setting methods",
        "O09_EvaluateGoals": "Meet the Instructor at the goal board and evaluate the example goals",
        "O10_ReturnToMali": "Visit Aunt Mali and check how reducing sugar is going",
        "O15_ReviewGoalPlan": "Return to Aunt Mali and review her completed goal plan",
        "O15B_RoadsidePractice": "Talk to the villager on the road",
        "O16_FinalDebrief": "Report back to the Instructor",
    }
    for objective_id, expected_text in expected_goal_tracker_text.items():
        actual_text = str(goal_objectives[objective_id].get_editor_property("objective_text"))
        if actual_text != expected_text:
            raise RuntimeError("Unexpected Quest 2 tracker text for {}: {}".format(
                objective_id, actual_text))
    for objective_id in (
            "O01_ReachHealthPost", "O04_LearnGoalSetting", "O06_ReflectOnDirection",
            "O08_IdentifyGoalMethods", "O09_EvaluateGoals", "O16_FinalDebrief"):
        objective_data = goal_objectives[objective_id]
        participant = str(unreal.GameplayTagLibrary.get_tag_name(
            objective_data.get_editor_property("participant_tag")))
        if (objective_data.get_editor_property("objective_type")
                != unreal.VHVQuestObjectiveType.CONVERSATION
                or participant != "VHV.Participant.Instructor"
                or objective_data.get_editor_property("auto_start")):
            raise RuntimeError("Instructor objective {} must use objective-gated Talk interaction".format(
                objective_id))
    if (goal_objectives["O02_ObserveGoalConversation"].get_editor_property("world_action_start_policy")
            != unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER):
        raise RuntimeError("Goal Setting initial observation must use Explicit Trigger")
    for objective_id in (
            "O01_ReachHealthPost", "O04_LearnGoalSetting", "O06_ReflectOnDirection",
            "O08_IdentifyGoalMethods", "O09_EvaluateGoals", "O10_ReturnToMali",
            "O15_ReviewGoalPlan", "O15B_RoadsidePractice", "O16_FinalDebrief"):
        if goal_objectives[objective_id].get_editor_property("auto_start"):
            raise RuntimeError("Quest 2 interaction objective {} must wait for player interaction".format(
                objective_id))
    for objective_id, counter_name in (
            ("O05_ComparePatients", "ComparisonObserved"),
            ("O07_ObserveGoalMethods", "MethodsObserved")):
        conditions = goal_objectives[objective_id].get_editor_property(
            "completion_conditions").get_editor_property("conditions")
        if len(conditions) != 1 or conditions[0].get_editor_property("compare_value") != 2:
            raise RuntimeError("{} must wait for both independently triggered scenes".format(objective_id))

    q2_departures = goal_objectives["O16_FinalDebrief"].get_editor_property("completion_npc_moves")
    q2_move_pairs = {
        (str(unreal.GameplayTagLibrary.get_tag_name(move.get_editor_property("participant_tag"))),
         str(unreal.GameplayTagLibrary.get_tag_name(move.get_editor_property("destination_location_tag"))))
        for move in q2_departures}
    if q2_move_pairs != {
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.ExercisePark"),
            ("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengCandidate")}:
        raise RuntimeError("Quest 2 completion must independently move Instructor and Aunt Saeng into Quest 3 staging")

    role_quest = next(quest for quest in quests
                      if str(quest.get_editor_property("quest_id")) == "Q_HBCT_03_ROLE_MODEL")
    role_start = role_quest.get_editor_property("start_stinger")
    role_complete = role_quest.get_editor_property("completion_stinger")
    if (str(role_start.get_editor_property("label")) != "TECHNIQUE 3"
            or str(role_start.get_editor_property("title")) != "ROLE MODEL"
            or str(role_start.get_editor_property("subtitle")) != "HEALTH BEHAVIOR CHANGE"):
        raise RuntimeError("Role Model start stinger is not authored correctly")
    if (str(role_complete.get_editor_property("label")) != "QUEST COMPLETE"
            or str(role_complete.get_editor_property("title")) != "ROLE MODEL"
            or str(role_complete.get_editor_property("subtitle")) != "TECHNIQUE 3"
            or role_quest.get_editor_property("auto_start_next_quest")):
        raise RuntimeError("Role Model completion must stop after Technique 3")
    role_objectives = {str(item.get_editor_property("objective_id")): item
                       for item in role_quest.get_editor_property("objectives")}
    expected_role_tracker = {
        "O01_ReachExercisePark": "Explore the exercise park",
        "O02_DiscoverRoleModels": "Find examples of people learning from others",
        "O03_ClassifyModels": "Talk to the Instructor about what you observed",
        "O04_MeetSomchai": "Talk to Uncle Somchai",
        "O05_UnderstandSomchai": "Find out what kind of role model could help Somchai",
        "O06_InspectCandidates": "Meet the possible role models",
        "O07_SelectRoleModel": "Recommend a role model for Somchai",
        "O08_ExplainModelFit": "Identify why the model is suitable",
        "O09_SeeChangeOverTime": "Follow how role modeling can influence behavior over time",
        "O10_ReconstructModeling": "Reconstruct how modeled behavior becomes a habit",
        "O11_PrepareDemonstration": "Prepare Somchai's role-model session",
        "O12_ObserveSaeng": "Listen to Aunt Saeng's experience",
        "O13_SupportImitation": "Help Somchai try the behavior",
        "O14_PracticeTogether": "Walk with Aunt Saeng and Somchai",
        "O15_FinalApplication": "Help another villager choose a useful model",
        "O16_FinalDebrief": "Talk to the Instructor",
    }
    for objective_id, expected_text in expected_role_tracker.items():
        if str(role_objectives[objective_id].get_editor_property("objective_text")) != expected_text:
            raise RuntimeError("Unexpected Quest 3 tracker text for {}".format(objective_id))
    for objective_id in ("O03_ClassifyModels", "O11_PrepareDemonstration", "O16_FinalDebrief"):
        objective_data = role_objectives[objective_id]
        participant = str(unreal.GameplayTagLibrary.get_tag_name(
            objective_data.get_editor_property("participant_tag")))
        if (participant != "VHV.Participant.Instructor"
                or objective_data.get_editor_property("auto_start")):
            raise RuntimeError("Quest 3 Instructor objective {} must wait for normal Talk interaction".format(
                objective_id))
    for objective_id, counter_value in (
            ("O02_DiscoverRoleModels", 4), ("O06_InspectCandidates", 3),
            ("O09_SeeChangeOverTime", 5)):
        conditions = role_objectives[objective_id].get_editor_property(
            "completion_conditions").get_editor_property("conditions")
        if len(conditions) != 1 or conditions[0].get_editor_property("compare_value") != counter_value:
            raise RuntimeError("Quest 3 counter objective {} has invalid threshold".format(objective_id))

    for objective_id in (
            "O01_ReachExercisePark", "O02_DiscoverRoleModels",
            "O03_ClassifyModels", "O05_UnderstandSomchai"):
        if role_objectives[objective_id].get_editor_property("completion_npc_moves"):
            raise RuntimeError("Aunt Saeng must not be repeatedly rerouted during {}".format(objective_id))

    expected_later_role_moves = {
        "O08_ExplainModelFit": {
            ("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengDemo"),
            ("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiDemo"),
        },
    }
    for objective_id, expected_moves in expected_later_role_moves.items():
        actual_moves = {
            (str(unreal.GameplayTagLibrary.get_tag_name(move.get_editor_property("participant_tag"))),
             str(unreal.GameplayTagLibrary.get_tag_name(
                 move.get_editor_property("destination_location_tag"))))
            for move in role_objectives[objective_id].get_editor_property("completion_npc_moves")}
        if actual_moves != expected_moves:
            raise RuntimeError("Quest 3 later Saeng/Somchai routing changed at {}".format(
                objective_id))

    if role_objectives["O12_ObserveSaeng"].get_editor_property("completion_npc_moves"):
        raise RuntimeError("The walking practice must begin as blocking objective movement, not a fire-and-forget completion move")

    def authored_stage_pairs(objective_id):
        result = []
        for stage in role_objectives[objective_id].get_editor_property("npc_move_stages"):
            result.append({
                (str(unreal.GameplayTagLibrary.get_tag_name(move.get_editor_property("participant_tag"))),
                 str(unreal.GameplayTagLibrary.get_tag_name(
                     move.get_editor_property("destination_location_tag"))))
                for move in stage.get_editor_property("moves")})
            if not all(move.get_editor_property("face_destination_rotation")
                       for move in stage.get_editor_property("moves")):
                raise RuntimeError("Walking-practice moves must apply authored destination facing")
        return result

    mid_stage = {
        ("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengWalkMid"),
        ("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiWalkMid"),
    }
    end_stage = {
        ("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengWalkEnd"),
        ("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiWalkEnd"),
    }
    if authored_stage_pairs("O13_SupportImitation"):
        raise RuntimeError("Somchai practice must remain interactable before walking begins")
    if authored_stage_pairs("O14_PracticeTogether") != [mid_stage, end_stage]:
        raise RuntimeError("Quest 3 walking objective must restore at Mid and then wait for both End arrivals")
    walk_objective = role_objectives["O14_PracticeTogether"]
    if walk_objective.get_editor_property("npc_travel_mode") != unreal.VHVQuestNPCTravelMode.FREE_ROAM:
        raise RuntimeError("Quest 3 walking objective must use free-roam NPC travel")
    walk_stages = walk_objective.get_editor_property("npc_move_stages")
    first_stage_receiver = str(unreal.GameplayTagLibrary.get_tag_name(
        walk_stages[0].get_editor_property("ambient_conversation_receiver_tag")))
    second_stage_receiver = str(unreal.GameplayTagLibrary.get_tag_name(
        walk_stages[1].get_editor_property("ambient_conversation_receiver_tag")))
    if (first_stage_receiver != "VHV.WorldReceiver.HBCT.RoleModel.WalkAlong"
            or second_stage_receiver not in ("", "None")):
        raise RuntimeError("Quest 3 walking speech must start once with the Mid travel stage")
    if not all(abs(move.get_editor_property("move_speed_override") - 180.0) < 0.01
               for stage in walk_stages for move in stage.get_editor_property("moves")):
        raise RuntimeError("Quest 3 free-roam walkers must use the authored 180 cm/s speed")
    if (role_objectives["O13_SupportImitation"].get_editor_property("auto_start")
            or not role_objectives["O14_PracticeTogether"].get_editor_property("auto_start")):
        raise RuntimeError("Somchai practice must be interactive and the end beat must auto-start after the walk")

    final_nodes = conversations["DA_Conversation_HBCT_FinalDebrief"].get_editor_property(
        "conversation").get_editor_property("nodes")
    if ([str(node.get_editor_property("node_id")) for node in final_nodes]
            != ["Final_01", "Final_02", "Final_03"]):
        raise RuntimeError("Final debrief still contains obsolete presentation activity nodes")


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


def validate_goal_setting_runtime(level):
    game_instance = unreal.new_object(unreal.GameInstance)
    subsystem = unreal.new_object(unreal.VHVTextbookSubsystem, outer=game_instance)
    subsystem.start_journey(level)
    subsystem.set_progression_mode(unreal.VHVTextbookProgressionMode.QUEST_MANAGED)

    def assert_empty_teaching_skips(activity_id, submit):
        if not subsystem.start_activity_by_id(activity_id):
            raise RuntimeError("Could not start {} for Quest 2 flow validation".format(activity_id))
        if not submit():
            raise RuntimeError("{} did not accept its correct answer".format(activity_id))
        if subsystem.get_current_phase() != unreal.LearningPhase.FEEDBACK:
            raise RuntimeError("{} did not reach Feedback".format(activity_id))
        subsystem.enter_teach_phase()
        if subsystem.is_activity_active():
            raise RuntimeError("{} exposed an empty Teach phase".format(activity_id))

    assert_empty_teaching_skips("ClearerPath", lambda: (subsystem.submit_answer(True), True)[1])
    assert_empty_teaching_skips("UsefulGoals", lambda: subsystem.submit_multi_choice([0, 1]))

    classification_answers = {
        "IdentifyMethods": [0, 2],
        "MaliGoalBoardA": [0, 1],
        "MaliGoalBoardB": [0, 1],
    }
    for activity_id, selected_answers in classification_answers.items():
        assert_empty_teaching_skips(
            activity_id,
            lambda answers=selected_answers: subsystem.submit_multi_choice(answers))

    for activity_id in ("WhatIsGoalSetting", "UsefulGoalCharacteristics"):
        if not subsystem.start_activity_by_id(activity_id):
            raise RuntimeError("Could not start intentional lesson {}".format(activity_id))
        if not subsystem.submit_observation():
            raise RuntimeError("Could not submit intentional lesson {}".format(activity_id))
        if (not subsystem.is_activity_active()
                or subsystem.get_current_phase() != unreal.LearningPhase.TEACH):
            raise RuntimeError("Intentional lesson {} showed blank Feedback or was collapsed".format(
                activity_id))
        subsystem.advance_teach()

def validate_observation_runtime(level):
    game_instance = unreal.new_object(unreal.GameInstance)
    subsystem = unreal.new_object(unreal.VHVTextbookSubsystem, outer=game_instance)
    subsystem.start_journey(level)
    subsystem.set_progression_mode(unreal.VHVTextbookProgressionMode.QUEST_MANAGED)

    if not subsystem.start_activity_by_id("IntroLesson") or not subsystem.submit_observation():
        raise RuntimeError("Passive Observation completion path failed")
    passive_state = subsystem.get_runtime_state()
    if (passive_state.get_editor_property("current_phase") != unreal.LearningPhase.TEACH
            or not passive_state.get_editor_property("activity_active")):
        raise RuntimeError("Passive Observation behavior changed")
    subsystem.advance_teach()

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
    validate_goal_setting_runtime(level)
    validate_observation_runtime(level)
    validate_arc_runtime(arc)
    validate_assets(all_assets)
    for asset in all_assets:
        save_asset(asset)
    configure_map(ambient_assets, conversations)
    configure_player_controller(level, arc)
    validate_map_and_defaults(level, arc)
    unreal.log("{} Authored {} activities, {} conversations, {} ambient sequences, 4 quests, and the production level bindings.".format(
        LOG, len(build_activities()) + len(build_goal_setting_activities()) + len(build_role_model_activities()),
        len(conversations), len(ambient_assets)))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
