# Validation and host checklist

## Automated gates

`GeigerEngineTests` checks all 37 preview parameter IDs against frozen numeric IDs, parameter-ID uniqueness, preset validity, silence, rate statistics, waiting-time variation, non-extendable dead-time throughput and minimum spacing, deterministic replay, block/span invariance, timing independence from click colour, eight distinct click-model outputs, mono symmetry, finite/bounded output under extreme automation, power-off settling and invalid inputs. Sample-rate coverage includes 1, 8, 22.05, 44.1, 96, 192, 384 and 768 kHz, plus fractional rates of 1000.25 and 48000.123 Hz. Activation is explicitly asserted so a silently rejected test rate cannot pass. Pulse tails are checked for non-finite and subnormal float samples without depending on host flush-to-zero settings.

Dense 12,000 cps tests at 44.1/48/96 kHz compare balanced and unbalanced renders, assert that detector counts do not change, and check finite output plus reduced energy/headroom usage. Every one of the 20 presets is checked for range validity, non-silence and name-matching semantics. The antialiased clipping law is tested for bounds, symmetry and nearly equal input stability.

`GeigerRealtimeTests` intercepts C++ heap allocation operators while exercising configuration, reset, gate changes, test clicks and sample rendering under extreme automation at five rates. It does not intercept arbitrary C `malloc` calls or replace source review.

`GeigerHostTests` instantiates the actual null-clap plug-in and exercises parameter discovery, sample-offset MIDI, MIDI gate, sustain/release, output in float/double host buffers, project-state round-trip and repeated activation at eight low, fractional and high sample rates. It also checks preset switching, preservation of performance controls, edited names, metadata state round-trips and loading a valid preview-format state with an empty extra-state payload. It is not a substitute for a real DAW.

`GeigerUiSmoke` creates all four real editor pages at 900x660, 1120x760 and 1600x1000, checks bounds and pairwise control non-overlap, and renders PNGs. It focuses every parameter and checks that preview data is finite, exercises the actual preset selector callback and confirms names survive editor recreation. It does not prove mouse interaction or high-DPI embedding in every host. Linux screenshots run under Xvfb. The UI-test artifacts allow visual inspection of actual rendered controls rather than mock-ups.

Both CI platforms run clap-validator 0.4.1, build `0.4.1-127-g152b982`, without test-exclusion flags. The validator may automatically skip optional-extension tests that do not apply to this instrument. Publication depends on every build job succeeding. Exact results are recorded in GitHub Actions for each release commit.

`GeigerRender` creates 20 named five-second stereo WAV examples, ordered like the factory presets. These are generated directly from the production engine at 48 kHz with short edge fades, not external recordings. No loudness normalization is applied. A PRESETS.txt index explains each example. The samples use the production engine rather than illustrative graph equations.

## Reported host status

The user confirmed the 0.1.0-preview.7 build works in Bitwig. This is a report about the preview, not proof that every new 1.0 sound, visualization or state transition has been manually checked. Keep automated and manual results distinct.

## Manual Bitwig checklist (not automated)

1. Install the CLAP, rescan, and insert it as an instrument. Confirm Free running produces sound with no MIDI or audio input. Some hosts suspend inactive tracks; press Play or arm/monitor the instrument when necessary.
2. Open/close the editor repeatedly; resize each page, including at 125%, 150% and 200% desktop scale. Check dropdowns, typed knob values, host parameter changes and probe-pad gestures.
3. Automate Intensity, Pitch, Tone and Output. Map the Performance remote page. Verify host undo/automation gestures and save/reopen a project.
4. Select Transport mode; verify silence after release when stopped and repeatable starts with Restart on play enabled. Free running should not require Play, provided the host continues processing the track.
5. Select MIDI gate. Try chords, repeated notes, note-on velocity zero, sustain, note-offs, all-notes-off and all-sound-off. Verify optional MIDI pitch is neutral at MIDI note 60.
6. Set Background and Intensity to zero, turn off Hiss/Hum, and wait for tails; verify silence. Try Test click, Silence and Power. Test click should not falsely increment detected-particle totals.
7. Browse presets with the selector and arrow buttons, edit a circuit parameter and check the asterisk, save/reopen and verify the name. Load a copy of a preview project. Hover controls on each tab and distinguish grey reference previews from real detector activity.
8. Compare mono default against stereo scatter; sweep all click/speaker choices and high densities at a low monitoring volume. Check CPU use in the actual project.
9. On Linux, confirm the host provides CLAP timer support and X11/XWayland embedding. There is no native Wayland editor fallback.

Do not claim these manual steps passed without actually performing them. Host-specific GUI, scanner and MIDI-routing behaviour remains a real-user verification task after automated validation.
