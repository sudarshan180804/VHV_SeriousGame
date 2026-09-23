"""Author the production HBCT prologue and Techniques 1–2.

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
    day = struct(
        unreal.DayData,
        day_number=1,
        day_title="First Day in the Village",
        narrative_role="Production HBCT prologue, Technique 1, and Technique 2",
        introduction_text="Meet the Instructor and begin by observing real life.",
        topics=[motivation_topic, goal_setting_topic],
        completion_summary="Building Motivation and Goal Setting complete.",
        completion_takeaways=[
            "Motivation comes from what matters personally.",
            "A VHV guides rather than commands.",
            "A small feasible plan turns readiness toward action.",
            "A useful goal turns intention into a clear behavior, target, and time frame.",
        ])
    level.set_editor_properties({
        "level_number": 1,
        "level_title": "HBCT — Building Motivation",
        "level_description": "Production prologue, market observation, Building Motivation, and Goal Setting content.",
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
                  entry_node_id="Final_01", auto_start=False),
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
        objective("O06_ReflectOnDirection", "Return to the Instructor and discuss why goals matter", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_GoalSetting_ComparisonReflection"],
                  entry_node_id="GSCompare_01", auto_start=False),
        objective("O07_ObserveGoalMethods", "Visit both demonstration areas and observe two ways of setting goals", q.CUSTOM_EVENT,
                  custom_event_tag=tag("VHV.CustomEvent.HBCT.GoalSetting.MethodsObserved"),
                  completion_conditions=completion_condition(
                      "VHV.Story.Counter.HBCT.GoalSetting.MethodsObserved", 2)),
        objective("O08_IdentifyGoalMethods", "Talk to the Instructor and distinguish self-set and jointly set goals", q.CONVERSATION,
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
                  entry_node_id="GSFinal_01", auto_start=False),
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
               auto_start_next_quest=False,
               start_stinger=major_stinger(
                   "TECHNIQUE 2", "GOAL SETTING", "HEALTH BEHAVIOR CHANGE", 1.9),
               completion_stinger=major_stinger(
                   "QUEST COMPLETE", "GOAL SETTING", "TECHNIQUE 2", 1.9)),
    ]
    arc.set_editor_properties({
        "quest_arc_id": "HBCT",
        "quest_arc_title": "Health Behavior Change Techniques",
        "quest_arc_description": "Production HBCT story arc containing the prologue, Building Motivation, and Goal Setting.",
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

    # Technique 2 uses a deliberately simple linear staging strip. These transforms
    # are production-editable defaults only; no quest logic depends on them.
    health_post_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_HealthPost", (3000.0, -2500.0, 100.0),
        (300.0, 300.0, 220.0), "VHV.Location.HBCT.GoalSetting.HealthPost",
        "Q_HBCT_02_GOAL_SETTING", "O01_ReachHealthPost")
    place_trigger_with_group_if_disconnected(
        health_post_trigger, [instructor], (-350.0, -260.0, 0.0))

    instructor_lesson_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_InstructorLesson", (3000.0, -2500.0, 100.0),
        (220.0, 220.0, 200.0), "VHV.Location.HBCT.GoalSetting.HealthPost",
        "Q_HBCT_02_GOAL_SETTING", "O04_LearnGoalSetting")
    place_trigger_with_group_if_disconnected(
        instructor_lesson_trigger, [instructor], (-175.0, -260.0, 0.0))

    instructor_reflection_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_InstructorReflection", (3000.0, -2500.0, 100.0),
        (220.0, 220.0, 200.0), "VHV.Location.HBCT.GoalSetting.HealthPost",
        "Q_HBCT_02_GOAL_SETTING", "O06_ReflectOnDirection")
    place_trigger_with_group_if_disconnected(
        instructor_reflection_trigger, [instructor], (0.0, -260.0, 0.0))

    instructor_methods_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_InstructorMethods", (3000.0, -2500.0, 100.0),
        (220.0, 220.0, 200.0), "VHV.Location.HBCT.GoalSetting.HealthPost",
        "Q_HBCT_02_GOAL_SETTING", "O08_IdentifyGoalMethods")
    place_trigger_with_group_if_disconnected(
        instructor_methods_trigger, [instructor], (175.0, -260.0, 0.0))

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

    good_goal_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_GoodGoalBoard", (7400.0, -2500.0, 100.0),
        (320.0, 280.0, 220.0), "VHV.Location.HBCT.GoalSetting.GoodGoalBoard",
        "Q_HBCT_02_GOAL_SETTING", "O09_EvaluateGoals")
    place_trigger_with_group_if_disconnected(
        good_goal_trigger, [instructor], (350.0, -260.0, 0.0))

    mali_check_in_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_MaliCheckIn", (8500.0, -2500.0, 100.0),
        (320.0, 280.0, 220.0), "VHV.Location.HBCT.GoalSetting.MaliCheckIn",
        "Q_HBCT_02_GOAL_SETTING", "O10_ReturnToMali")
    place_trigger_with_group_if_disconnected(
        mali_check_in_trigger, [mali], (-180.0, -260.0, 0.0))

    mali_plan_review_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_MaliPlanReview", (8500.0, -2500.0, 100.0),
        (260.0, 240.0, 220.0), "VHV.Location.HBCT.GoalSetting.MaliCheckIn",
        "Q_HBCT_02_GOAL_SETTING", "O15_ReviewGoalPlan")
    place_trigger_with_group_if_disconnected(
        mali_plan_review_trigger, [mali], (180.0, -260.0, 0.0))

    final_instructor_trigger = configure_objective_trigger(
        "HBCT_Trigger_GoalSetting_FinalInstructor", (9600.0, -2500.0, 100.0),
        (320.0, 280.0, 220.0), "VHV.Location.HBCT.GoalSetting.FinalInstructor",
        "Q_HBCT_02_GOAL_SETTING", "O16_FinalDebrief")
    place_trigger_with_group_if_disconnected(
        final_instructor_trigger, [instructor], (525.0, -260.0, 0.0))

    expected_goal_trigger_labels = {
        "HBCT_Trigger_GoalSetting_HealthPost",
        "HBCT_Trigger_GoalSetting_InstructorLesson",
        "HBCT_Trigger_GoalSetting_InstructorReflection",
        "HBCT_Trigger_GoalSetting_InstructorMethods",
        "HBCT_Trigger_GoalSetting_InitialObservation",
        "HBCT_Trigger_GoalSetting_ClearGoal",
        "HBCT_Trigger_GoalSetting_NoGoal",
        "HBCT_Trigger_GoalSetting_SelfSet",
        "HBCT_Trigger_GoalSetting_Joint",
        "HBCT_Trigger_GoalSetting_GoodGoalBoard",
        "HBCT_Trigger_GoalSetting_MaliCheckIn",
        "HBCT_Trigger_GoalSetting_MaliPlanReview",
        "HBCT_Trigger_GoalSetting_FinalInstructor",
    }
    for actor in list(editor_actors().get_all_level_actors()):
        label = actor.get_actor_label()
        if (actor.get_class() == unreal.VHVQuestLocationVolume.static_class()
                and label.startswith("HBCT_Trigger_GoalSetting_")
                and label not in expected_goal_trigger_labels):
            unreal.log("{} Removing obsolete Quest 2 trigger {}".format(LOG, label))
            editor_actors().destroy_actor(actor)

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
        "HBCT_Trigger_GoalSetting_HealthPost",
        "HBCT_Trigger_GoalSetting_InstructorLesson",
        "HBCT_Trigger_GoalSetting_InstructorReflection",
        "HBCT_Trigger_GoalSetting_InstructorMethods",
        "HBCT_Trigger_GoalSetting_InitialObservation",
        "HBCT_Trigger_GoalSetting_ClearGoal",
        "HBCT_Trigger_GoalSetting_NoGoal",
        "HBCT_Trigger_GoalSetting_SelfSet",
        "HBCT_Trigger_GoalSetting_Joint",
        "HBCT_Trigger_GoalSetting_GoodGoalBoard",
        "HBCT_Trigger_GoalSetting_MaliCheckIn",
        "HBCT_Trigger_GoalSetting_MaliPlanReview",
        "HBCT_Trigger_GoalSetting_FinalInstructor",
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
    }
    if set(participant_tags) != expected_participants:
        raise RuntimeError("Unexpected production quest participant set: {}".format(participant_tags))

    actors_by_label = {actor.get_actor_label(): actor for actor in actors}
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
        "HBCT_Trigger_GoalSetting_HealthPost": ("O01_ReachHealthPost", True, "", ["HBCT_Intro_Instructor"]),
        "HBCT_Trigger_GoalSetting_InstructorLesson": ("O04_LearnGoalSetting", True, "", ["HBCT_Intro_Instructor"]),
        "HBCT_Trigger_GoalSetting_InstructorReflection": ("O06_ReflectOnDirection", True, "", ["HBCT_Intro_Instructor"]),
        "HBCT_Trigger_GoalSetting_InstructorMethods": ("O08_IdentifyGoalMethods", True, "", ["HBCT_Intro_Instructor"]),
        "HBCT_Trigger_GoalSetting_InitialObservation": ("O02_ObserveGoalConversation", False, "VHV.WorldReceiver.HBCT.GoalSetting.InitialObservation", ["HBCT_GoalSetting_ObservationVHV", "HBCT_GoalSetting_AuntieNuan"]),
        "HBCT_Trigger_GoalSetting_ClearGoal": ("O05_ComparePatients", False, "VHV.WorldReceiver.HBCT.GoalSetting.ClearGoal", ["HBCT_GoalSetting_Nuan_ClearGoal"]),
        "HBCT_Trigger_GoalSetting_NoGoal": ("O05_ComparePatients", False, "VHV.WorldReceiver.HBCT.GoalSetting.NoGoal", ["HBCT_GoalSetting_UncleChai"]),
        "HBCT_Trigger_GoalSetting_SelfSet": ("O07_ObserveGoalMethods", False, "VHV.WorldReceiver.HBCT.GoalSetting.SelfSet", ["HBCT_GoalSetting_SelfSetVHV", "HBCT_GoalSetting_SelfSetPatient"]),
        "HBCT_Trigger_GoalSetting_Joint": ("O07_ObserveGoalMethods", False, "VHV.WorldReceiver.HBCT.GoalSetting.Joint", ["HBCT_GoalSetting_JointVHV", "HBCT_GoalSetting_JointPatient"]),
        "HBCT_Trigger_GoalSetting_GoodGoalBoard": ("O09_EvaluateGoals", True, "", ["HBCT_Intro_Instructor"]),
        "HBCT_Trigger_GoalSetting_MaliCheckIn": ("O10_ReturnToMali", True, "", ["HBCT_Motivation_Mali"]),
        "HBCT_Trigger_GoalSetting_MaliPlanReview": ("O15_ReviewGoalPlan", True, "", ["HBCT_Motivation_Mali"]),
        "HBCT_Trigger_GoalSetting_FinalInstructor": ("O16_FinalDebrief", True, "", ["HBCT_Intro_Instructor"]),
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
    if len(authored_activities) != 26 or len(activity_ids) != 26:
        raise RuntimeError("HBCT production content must contain 26 uniquely addressed activities")

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

    def has_meaningful_teaching(teaching_data):
        return bool(
            str(teaching_data.get_editor_property("title")).strip()
            or str(teaching_data.get_editor_property("content")).strip()
            or any(str(value).strip() for value in teaching_data.get_editor_property("key_takeaways"))
            or teaching_data.get_editor_property("media")
            or teaching_data.get_editor_property("media_texture")
            or str(teaching_data.get_editor_property("media_caption")).strip())

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
    }
    if len(quests) != 3:
        raise RuntimeError("HBCT transition shell must contain exactly three quests")
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
        "O06_ReflectOnDirection": "Return to the Instructor and discuss why goals matter",
        "O07_ObserveGoalMethods": "Visit both demonstration areas and observe two ways of setting goals",
        "O08_IdentifyGoalMethods": "Talk to the Instructor and distinguish self-set and jointly set goals",
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
    if (goal_objectives["O02_ObserveGoalConversation"].get_editor_property("world_action_start_policy")
            != unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER):
        raise RuntimeError("Goal Setting initial observation must use Explicit Trigger")
    for objective_id in (
            "O01_ReachHealthPost", "O04_LearnGoalSetting", "O06_ReflectOnDirection",
            "O08_IdentifyGoalMethods", "O09_EvaluateGoals", "O10_ReturnToMali",
            "O15_ReviewGoalPlan", "O15B_RoadsidePractice", "O16_FinalDebrief"):
        if goal_objectives[objective_id].get_editor_property("auto_start"):
            raise RuntimeError("Location-based Quest 2 objective {} must wait for its trigger".format(
                objective_id))
    for objective_id, counter_name in (
            ("O05_ComparePatients", "ComparisonObserved"),
            ("O07_ObserveGoalMethods", "MethodsObserved")):
        conditions = goal_objectives[objective_id].get_editor_property(
            "completion_conditions").get_editor_property("conditions")
        if len(conditions) != 1 or conditions[0].get_editor_property("compare_value") != 2:
            raise RuntimeError("{} must wait for both independently triggered scenes".format(objective_id))

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
    configure_map(ambient_assets)
    configure_player_controller(level, arc)
    validate_map_and_defaults(level, arc)
    unreal.log("{} Authored {} activities, {} conversations, {} ambient sequences, 3 quests, and the production level bindings.".format(
        LOG, len(build_activities()) + len(build_goal_setting_activities()),
        len(conversations), len(ambient_assets)))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
