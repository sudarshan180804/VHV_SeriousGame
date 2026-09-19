# VHV UI Style Guide

Status: canonical documentation of the approved SingleChoice and MultiChoice runtime UI.

The code-level source of truth is `Source/VHV/UI/Textbook/VHVActivityUIStyle.h`. This document records the values consumed by `UVHVQuestionWidget` and `SVHVChoiceCard` exactly as they exist in source. Values described as hard-coded are intentionally documented rather than normalized or moved in this pass.

## Design direction

VHV UI is a premium, restrained, modern adventure-game interface designed to sit over a realistic Unreal Engine Thai village environment.

Its core characteristics are:

- realistic-world-first composition
- dark smoked translucent glass
- muted champagne-gold accents
- warm off-white typography
- large, soft rounded geometry
- subtle borders and restrained shadows
- generous spacing and minimal clutter
- a natural, lively, professional character
- never sci-fi
- never mobile-app-like
- never generic e-learning software
- never default Unreal prototype styling

All future VHV widgets should reuse these parameters unless a specific usability requirement warrants a deviation. This applies to Ordering, Matching, Observation, DialogueChoice, Feedback, Conversation/Dialogue, Quest Tracker, Journal, menus, and prompts. Use `VHVActivityUIStyle` as the code-level source of truth wherever practical; this guide is the human-readable source of truth.

## Units and color notation

- Geometry, padding, radii, font sizes, and animation distances are Slate units unless stated otherwise.
- Colors constructed with `FromSRGB(R,G,B,A)` use `FLinearColor::FromSRGBColor`, then explicitly set alpha to `A / 255`.
- The linear values below are the resulting runtime values, rounded to six decimal places for documentation.
- `GlassMain` is authored directly as a linear `FLinearColor`; its hex value is therefore only its display-sRGB equivalent, not its source representation.
- Rounded surfaces use `FSlateRoundedBoxBrush`.

## Canonical color palette

| Name | sRGB / hex | Runtime linear RGBA | Alpha | Current usage |
|---|---:|---:|---:|---|
| `GlassMain` | display equivalent `#3C4045` | `(0.045000, 0.052000, 0.060000, 0.610000)` | `0.61` | Unified question/activity panel; compatibility alias `CharcoalGlass` |
| `GlassCard` | `#1A1F23` | `(0.010330, 0.013702, 0.016807, 0.570000)` | `0.57` | Normal option pill; source overrides the alpha after sRGB conversion |
| `GlassHover` | `#1D2327` | `(0.012286, 0.016807, 0.020289, 0.592157)` | `151/255` | Hovered option pill |
| `GlassFocused` | `#23221E` | `(0.016807, 0.015996, 0.012983, 0.619608)` | `158/255` | Focused, unselected option pill |
| `GlassSelected` | `#2C261D` | `(0.025187, 0.019382, 0.012286, 0.650980)` | `166/255` | Selected option pill |
| `TextPrimary` | `#F0ECE4` | `(0.871367, 0.838799, 0.775822, 1.000000)` | `1.0` | Question, active answer, keycap, and indicator text |
| `TextSecondary` | `#C6C0B5` | `(0.564712, 0.527115, 0.462077, 0.850980)` | `217/255` | Instruction, inactive answer, legend labels |
| `TextMuted` | no separate token | same as `TextSecondary` | `217/255` | The current UI uses `TextSecondary` for muted text; disabled text uses `DisabledText` |
| `DisabledText` | `#7B7C79` | `(0.198069, 0.201556, 0.191202, 0.745098)` | `190/255` | Explicit disabled answer text |
| `WarningText` | `#E0A369` | `(0.745404, 0.366253, 0.141263, 1.000000)` | `1.0` | Missing-selection instruction |
| `GoldPrimary` | `#E0BB69` | `(0.745404, 0.496933, 0.141263, 1.000000)` | `1.0` | Canonical gold; neutral feedback accent |
| `GoldSelected` | `#E1BB6B` | `(0.752942, 0.496933, 0.147027, 1.000000)` | `1.0` | Selected indicator text; aliases `GoldActive` and `ActiveGold` |
| `GoldDivider` / `DividerGold` | `#D7B365` | `(0.679542, 0.450786, 0.130136, 0.721569)` | `184/255` | Header divider |
| `HeaderGold` | `#E0BB69` | `(0.745404, 0.496933, 0.141263, 0.850980)` | `217/255` | Emblem outline/glyph and activity heading |
| `BorderPanel` / `PanelBorder` | `#BEB39E` | `(0.514918, 0.450786, 0.341914, 0.270588)` | `69/255` | Question-panel border; alias `SoftGold` |
| `BorderNormal` / `BorderNeutral` | `#C3BEB3` | `(0.545724, 0.514918, 0.450786, 0.321569)` | `82/255` | Normal card, indicator, and keycap border |
| `BorderFocused` | `#D6BE91` | `(0.672443, 0.514918, 0.283149, 0.521569)` | `133/255` | Focused card and indicator border |
| `BorderSelected` / `BorderGold` | `#E1BB6B` | `(0.752942, 0.496933, 0.147027, 0.878431)` | `224/255` | Selected card and indicator border |
| `IndicatorNormal` | `#14181B` | `(0.006995, 0.009134, 0.010960, 0.568627)` | `145/255` | Normal, hovered, and focused indicator fill |
| `IndicatorFocused` | no separate fill token | `IndicatorNormal` fill plus `BorderFocused` | see constituent tokens | Focus changes the ring, not the indicator fill |
| `IndicatorSelected` | `#2C261D` | `(0.025187, 0.019382, 0.012286, 0.690196)` | `176/255` | Selected indicator fill |
| `SelectedGlow` | `#A17736` | `(0.356400, 0.184475, 0.036889, 0.098039)` | `25/255` | Selected pill and indicator shadow layers |
| `Positive` / `PositiveMuted` | `#68996F` | `(0.138432, 0.318547, 0.158961, 1.000000)` | `1.0` | Canonical positive token; currently not consumed by `VHVFeedbackWidget` |
| `Negative` / `NegativeMuted` | `#B5695B` | `(0.462077, 0.141263, 0.104616, 1.000000)` | `1.0` | Canonical negative token; currently not consumed by `VHVFeedbackWidget` |
| `ShadowPanel` | n/a; direct linear black | `(0.000000, 0.000000, 0.000000, 0.200000)` | `0.20` | Question-panel shadow layer |
| `ShadowCard` | n/a; direct linear black | `(0.000000, 0.000000, 0.000000, 0.170000)` | `0.17` | Option-pill and indicator shadow layers |

Additional runtime-only colors outside the style helper are listed under **Hard-coded values outside `VHVActivityUIStyle`**.

## Question panel

The question presentation is one unified rounded translucent glass `SBorder`, with a separate rounded shadow behind it. The content tree itself has no visible inner background and therefore adds no square inner slab. No `SBackgroundBlur` is used by the question widget.

| Parameter | Exact value |
|---|---:|
| Canvas anchors | left `0.1275`, top `0.655`, right `0.5625`, bottom `0.865` |
| Normalized width / height | `0.4350` / `0.2100` of viewport |
| Fill | `GlassMain` = direct linear `(0.045, 0.052, 0.060, 0.61)` |
| Corner radius | `26` |
| Border | `PanelBorder`, width `1` |
| Internal padding | `32` on all sides |
| Shadow color | `ShadowPanel` = linear black, alpha `0.20` |
| Shadow radius | `29` (`QuestionPanelRadius + 3`) |
| Shadow slot padding | `(left 5, top 6, right -5, bottom -6)` |
| Background blur | none |

The header is the first auto-height row. The divider follows it; the question and instruction are separate auto-height rows. Exact vertical relationships are:

- emblem-to-heading horizontal gap: `12`
- divider slot margins: left `32`, top `9`, right `0`, bottom `22`
- question line-height percentage: `1.12`
- instruction top margin from the question row: `24`
- no explicit instruction line-height override

At 1920x1080 before any platform DPI application scale, the anchored panel rectangle is `x=244.8..1080.0`, `y=707.4..934.2`, with a size of `835.2 x 226.8` pixels.

## Header

| Parameter | Exact value |
|---|---:|
| Activity emblem diameter | `19` |
| Emblem rounded-brush radius | `10` |
| Emblem fill | hard-coded `#1C1A14`, alpha `128/255` |
| Emblem border | `HeaderGold`, width `1` |
| Emblem glyph | literal `?` |
| Emblem glyph font | CoreStyle `Bold`, size `12` |
| Emblem glyph color | `HeaderGold` |
| Emblem-to-heading gap | `12` |
| Heading font | CoreStyle `Bold`, size `18` |
| Heading color | `HeaderGold` |
| Heading strings | `SINGLE CHOICE` or `MULTIPLE CHOICE` |
| Divider width / height | `205 x 1` |
| Divider brush radius | `1` |
| Divider color | `DividerGold` = `#D7B365`, alpha `184/255` |
| Divider position relative to heading | left indent `32`, top gap `9` |
| Divider-to-question gap | divider slot bottom margin `22` |

## Typography

The approved UI does not reference a project font asset. It resolves fonts through Unreal CoreStyle at runtime:

- `RegularFont(Size)` calls `FCoreStyle::GetDefaultFontStyle("Regular", Size)`.
- `MediumFont(Size)` calls `FCoreStyle::GetDefaultFontStyle("Bold", Size)`. The helper name says “Medium,” but the actual runtime style key is `Bold`.

| Element | Runtime font | Size | Color | Wrapping / line height |
|---|---|---:|---|---|
| Activity heading | CoreStyle `Bold` | `18` | `HeaderGold` | no explicit wrapping |
| Emblem glyph | CoreStyle `Bold` | `12` | `HeaderGold` | no explicit wrapping |
| Question | CoreStyle `Regular` | `30` | `TextPrimary` | auto-wrap; line height `1.12` |
| Instruction | CoreStyle `Regular` | `17` | `TextSecondary` | auto-wrap; default line height |
| Answer | CoreStyle `Regular` | `24` | state-dependent | auto-wrap; line height `1.08` |
| Indicator letter/check | CoreStyle `Bold` | `20` | state-dependent | centered |
| Keycap text | CoreStyle `Bold` | `13` | `TextPrimary` | no explicit wrapping |
| Legend label | CoreStyle `Regular` | `14` | `TextSecondary` | no explicit wrapping |

The instruction switches to `WarningText` without changing font metrics when submission is attempted with no selection.

## Option-pill geometry

Each `SVHVChoiceCard` is a single full-width rounded pill. The circular indicator is an internal leading child; it has no negative padding, translation, overlap, or protrusion.

| Parameter | Exact value |
|---|---:|
| Row height | `82` |
| Row width | fills the choice-stack width; no fixed `WidthOverride` |
| Choice-stack normalized width | `0.3125` of viewport |
| Pill radius | `41` |
| Left content inset | `8` |
| Right content padding | `30` (hard-coded in `SVHVChoiceCard.cpp`) |
| Top / bottom content inset | `8` / `8` |
| Indicator diameter / radius | `66` / `33` |
| Indicator left inset | `8` |
| Indicator top / bottom inset | `(82 - 66) / 2 = 8` / `8` |
| Indicator protrusion | `0`; fully internal |
| Indicator-to-answer gap | `30` |
| Answer-text start from pill left | `8 + 66 + 30 = 104` |
| Row-to-row gap | `17` after every row except the last |
| Answer font / line height | CoreStyle `Regular` `24`; `1.08` |
| Pill shadow radius | `43` (`ChoiceCardRadius + 2`) |
| Pill shadow slot padding | `(left 3, top 3, right -3, bottom -4)` |
| Indicator shadow radius | `33` |

The geometry is deliberately coordinated:

- The pill radius is exactly half the `82`-unit row height, producing capsule ends.
- The `66`-unit circle plus `8` units above and below exactly fills the `82`-unit height.
- The circle is fully inside the outer pill, with the same `8`-unit visual inset on the left, top, and bottom.
- The answer begins `30` units after the indicator, leaving an intentional text gap.

At 1920x1080 before platform DPI application scale, the choice-stack anchor width is `600` pixels. The pill uses that full allotted width.

## Option visual states

### Normal

- Pill fill: `GlassCard`.
- Pill outline: `BorderNeutral`, width `1`.
- Indicator fill: `IndicatorNormal`.
- Indicator outline: `BorderNeutral`, width `1`.
- Inactive answer text: `TextSecondary`.
- SingleChoice indicator letter: `TextPrimary`.
- MultiChoice indicator: empty.
- Pill/indicator shadow: `ShadowCard`.

### Hovered

- Pill fill changes to `GlassHover`.
- Pill outline remains `BorderNeutral`, width `1`.
- Indicator remains `IndicatorNormal` with `BorderNeutral`.
- Answer text changes to `TextPrimary`.
- No hover translation or scale is applied.
- Emphasis target is `0.38`, but the normal target outline width is still `1`, so this does not alter the converged outline width.

### Focused, not selected

- Pill fill: `GlassFocused`.
- Pill outline: `BorderFocused`.
- Focus outline target: `1.25`; the card’s interpolated steady-state width is `lerp(1.0, 1.25, 0.72) = 1.18` because focused emphasis targets `0.72`.
- Indicator fill: `IndicatorNormal`.
- Indicator outline: `BorderFocused`, width `1.25`.
- Answer and indicator text: `TextPrimary`.
- Final horizontal render translation: `+3` units.
- No gold-filled indicator and no selected glow.

### Selected

- Pill fill: `GlassSelected`.
- Pill outline: `BorderGold`, width `1.75`.
- Indicator fill: `IndicatorSelected`.
- Indicator outline: `BorderGold`, width `1.75`.
- Answer text: `TextPrimary`.
- Indicator letter/check: `GoldSelected`.
- Pill and indicator shadow layers change from `ShadowCard` to `SelectedGlow`.
- Selected emphasis target: `1.0`.

### Focused and selected

Selection has precedence for fill, outline, indicator, text, and shadow. Focus adds only the final `+3` horizontal translation; it does not replace the selected visuals.

### Disabled

- Pill fill: hard-coded `#191D20`, alpha `118/255`; linear `(0.009721, 0.012286, 0.014444, 0.462745)`.
- Pill outline: hard-coded `#7B7C79`, alpha `58/255`; linear `(0.198069, 0.201556, 0.191202, 0.227451)`.
- Answer text: `DisabledText`.
- `SetEnabled(false)` also applies Slate’s inherited disabled state.
- There is no separate explicit disabled indicator brush in `SVHVChoiceCard`; its indicator brush continues to follow selected/focused/normal state construction.

## SingleChoice and MultiChoice indicator semantics

Both modes use the same `66`-unit circular geometry and the same visual state brushes.

- SingleChoice displays an uppercase index letter (`A`, `B`, `C`, `D`, and onward). Selection keeps the letter and changes it to `GoldSelected` inside the selected indicator.
- MultiChoice displays no glyph while unselected. Selection displays Unicode check mark `U+2713` (`✓`) in `GoldSelected` inside the selected indicator.
- MultiChoice selection state is held independently for each option, allowing multiple selected pills to retain the selected visual simultaneously.

## Input legend

The legend is an auto-height row directly below the option container. It is right-aligned within the answer-panel anchor.

| Parameter | Exact value |
|---|---:|
| Distance below final card | `14` top padding (`LegendGap`) |
| Right inset | `8` |
| Keycap radius | `6` |
| Keycap fill | hard-coded `#1D2226`, alpha `145/255`; linear `(0.012286, 0.015996, 0.019382, 0.568627)` |
| Keycap border | `BorderNeutral`, width `1` |
| Keycap fixed size | none; auto-sized to content |
| Keycap inner padding | horizontal `8`, vertical `3` |
| Key text | CoreStyle `Bold` `13`, `TextPrimary` |
| Label text | CoreStyle `Regular` `14`, `TextSecondary` |
| Keycap-to-label gap | `7` |
| First label-to-ENTER gap | `20` |

Rendered content:

- SingleChoice: keycap `E`, label `Select`, keycap `ENTER`, label `Confirm`.
- MultiChoice: keycap `E`, label `Toggle`, keycap `ENTER`, label `Confirm`.

There are no literal bracket glyphs; the rounded keycap surfaces provide the visual `[E]` and `[ENTER]` treatment.

## Approved 16:9 screen composition

### Question panel

- left `0.1275` (`12.75%`)
- right `0.5625` (`56.25%`)
- top `0.6550` (`65.50%`)
- bottom `0.8650` (`86.50%`)
- width `0.4350` (`43.50%`)
- height `0.2100` (`21.00%`)

### Choice stack and legend region

- left `0.6625` (`66.25%`)
- right `0.9750` (`97.50%`)
- top `0.5100` (`51.00%`)
- anchor-region bottom `0.9400` (`94.00%`)
- width `0.3125` (`31.25%`)
- anchor-region height `0.4300` (`43.00%`)

With four options, the option rows occupy `4 x 82 + 3 x 17 = 379` Slate units. At 1080p and application scale `1.0`, they run from `y=550.8` to approximately `y=929.8`, or `51.00%` to approximately `86.09%` of the viewport. The legend begins `14` units below the final card and is aligned to the right with an additional `8`-unit right inset.

### Responsive behavior

- The two major regions use `SConstraintCanvas` fractional anchors, so their placement and allotted width/height track viewport proportions at 1920x1080, 2560x1440, and 1280x720.
- The pill fills the width allotted by the `31.25%` answer region; it has no fixed width.
- Text wraps in the question, instruction, and answer fields.
- Radii, padding, heights, gaps, and font sizes are Slate units and therefore pass through Unreal’s viewport application/DPI scaling.
- No project-specific DPI rule or curve is declared in the checked-in `Config` files. Runtime scaling therefore follows the active Unreal/viewport DPI configuration rather than a widget-local scale override.

## Glass, shadow, and depth construction

The approved question UI does not use a formal blurred drop shadow. Depth is produced by rounded translucent brushes layered behind the surfaces.

### Question panel

- Surface alpha: `0.61`.
- Shadow: black alpha `0.20`, rounded radius `29`.
- Shadow geometry: separate `SBorder` under the panel using slot padding `(5, 6, -5, -6)`.
- Background blur: none.

### Option pills

- Normal surface alpha: `0.57`.
- Normal shadow: black alpha `0.17`, rounded radius `43`.
- Shadow geometry: separate `SBorder` under the pill using slot padding `(3, 3, -3, -4)`.
- Selected shadow/illumination: `SelectedGlow`, gold alpha `25/255` (`0.098039`), radius `43`.
- Background blur: none.

### Indicators

- Indicator shadow radius: `33`.
- Normal color: `ShadowCard`.
- Selected color: `SelectedGlow`.
- The shadow and indicator occupy the same `66 x 66` overlay geometry; there is no explicit shadow offset.

## Animation parameters

| Element | Exact behavior |
|---|---|
| Question panel entrance | duration `0.21 s`; opacity `0 -> 1`; vertical translation `+12 -> 0`; `FMath::InterpEaseOut`, exponent `3` |
| Answer-panel entrance | duration `0.21 s`; opacity `0 -> 1`; no panel-level translation; same ease-out exponent `3` |
| Choice-card entrance | duration `0.21 s`; opacity `0 -> 1`; horizontal translation `+16 -> 0` (or `+3` if focused); ease-out exponent `3` |
| Choice-card stagger | option index multiplied by `0.03 s` |
| Focus terminal offset | `+3` horizontal Slate units |
| State emphasis interpolation | `FMath::FInterpTo` at speed `1 / 0.12 = 8.333333...` per second |
| Hover emphasis target | `0.38` |
| Focus emphasis target | `0.72` |
| Selected emphasis target | `1.0` |

There are no independent fixed-duration hover or selection animations. State fill, outline color, indicator fill, and shadow target switch on the next tick. The card outline width is the property smoothed by the emphasis interpolation. There is no scale or bounce animation.

For compatibility, the current feedback panel has its own hard-coded entrance duration of `0.20 s`, vertical translation `+14 -> 0`, and the same ease-out exponent `3`.

## Input and interaction standard

### SingleChoice

- `W` / Up Arrow: previous option.
- `S` / Down Arrow: next option.
- semantic `IA_Interact` action, currently mapped to `E`: select the focused option.
- left mouse click: select the clicked option exactly once on mouse release within the card.
- Enter: confirm and submit.
- Gamepad D-pad/left-stick up and down navigate; Face Button Bottom confirms.

### MultiChoice

- `W` / Up Arrow: previous option.
- `S` / Down Arrow: next option.
- semantic `IA_Interact` action, currently mapped to `E`: toggle the focused option.
- Space: secondary toggle shortcut.
- left mouse click: toggle the clicked option exactly once on mouse release within the card.
- Enter: confirm and submit.
- Gamepad Face Button Left toggles; Face Button Bottom confirms.

### Modal ownership rules

- `UVHVUIManagerComponent` owns modal input state and focus.
- Modal activities use `FInputModeGameAndUI`, do not lock the mouse to the viewport, keep the cursor visible during capture, show the mouse cursor, and focus the active modal widget.
- The UI manager calls both `SetUserFocus` and `SetKeyboardFocus`, then repeats focus assignment on the next tick to prevent stale focus during consecutive activity transitions.
- Modal state resets and applies move/look ignore input and disables character movement. Gameplay restoration switches to `FInputModeGameOnly`, hides the cursor, and restores walking movement.
- `IA_Interact` remains a semantic Enhanced Input action. `AVHVCharacter` receives its `Started` event, asks the UI manager to handle activity interaction first, and returns before world interaction when the modal consumes it. Widgets must not replace this route with a raw `E` binding.
- `LearningAsk` consumes the semantic interaction press even for activity types without a select operation, preventing simultaneous world interaction.
- After mouse selection, `UVHVQuestionWidget` restores keyboard focus to itself.
- Submitting with no selected answer does not submit. It changes the instruction to the appropriate selection-required message in `WarningText` and restores modal focus.

## Values still hard-coded outside `VHVActivityUIStyle`

These values are part of the approved current appearance but are not centralized yet.

### `SVHVChoiceCard.cpp`

- card right content padding `30`
- pill shadow slot padding `(3, 3, -3, -4)`
- entrance X translation `16`
- focused X translation `3`
- ease-out exponent `3`
- indicator font size `20`
- answer line-height percentage `1.08`
- focused outline width target `1.25`
- hover/focus/selected emphasis targets `0.38`, `0.72`, `1.0`
- disabled fill `#191D20`, alpha `118/255`
- disabled outline `#7B7C79`, alpha `58/255`

### `VHVQuestionWidget.cpp`

- question anchors `(0.1275, 0.655, 0.5625, 0.865)`
- answer anchors `(0.6625, 0.51, 0.975, 0.94)`
- question shadow slot padding `(5, 6, -5, -6)`
- keycap fill `#1D2226`, alpha `145/255`, radius `6`
- emblem fill `#1C1A14`, alpha `128/255`, radius `10`
- emblem glyph `?`, font size `12`
- emblem-to-heading gap `12`
- divider margins `(32, 9, 0, 22)`
- question line-height percentage `1.12`
- instruction top gap `24`
- legend right inset `8`
- keycap padding `(8, 3)`
- keycap font size `13`; legend label size `14`
- keycap-to-label gap `7`; first-label-to-ENTER gap `20`
- panel entrance Y translation `12`
- choice entrance X translation `16` is implemented in `SVHVChoiceCard.cpp`
- ease-out exponent `3`

### `VHVFeedbackWidget.cpp` compatibility values

The feedback widget already consumes `CharcoalGlass`, `SoftGold`, `Gold`, `PrimaryText`, and `SecondaryText` aliases from the shared style, but it retains these local values:

- anchors `(0.045, 0.69, 0.49, 0.925)`
- panel radius `16`; initial border width `1`
- feedback shadow direct linear black alpha `0.42`, radius `17`
- shadow slot padding `(7, 9, -7, -9)`
- `SBackgroundBlur`: radius `4`, strength `5`; `PanelBrush` is the low-quality fallback
- panel padding `(30, 23, 32, 24)`
- title CoreStyle `Bold` size `16`
- body CoreStyle `Regular` size `25`, line height `1.12`
- body slot top/bottom padding `14` / `12`
- continue text CoreStyle `Regular` size `14`
- feedback entrance `0.20 s`, `+14 -> 0` Y, ease-out exponent `3`
- refreshed panel border width `1.2`, with accent alpha `0.72`
- positive feedback accent direct linear `(0.43, 0.76, 0.54, 1.0)`
- caution feedback accent direct linear `(0.88, 0.43, 0.30, 1.0)`

The feedback widget’s hard-coded positive and caution accents are not the `PositiveMuted` and `NegativeMuted` tokens. This guide records that difference without changing it.

## Runtime implementation references

- `Source/VHV/UI/Textbook/VHVActivityUIStyle.h`
- `Source/VHV/UI/Textbook/SVHVChoiceCard.h`
- `Source/VHV/UI/Textbook/SVHVChoiceCard.cpp`
- `Source/VHV/UI/Textbook/VHVQuestionWidget.h`
- `Source/VHV/UI/Textbook/VHVQuestionWidget.cpp`
- `Source/VHV/UI/Textbook/VHVFeedbackWidget.h`
- `Source/VHV/UI/Textbook/VHVFeedbackWidget.cpp`
- `Source/VHV/UI/VHVUIManagerComponent.h`
- `Source/VHV/UI/VHVUIManagerComponent.cpp`
- `Source/VHV/VHVCharacter.h`
- `Source/VHV/VHVCharacter.cpp`
