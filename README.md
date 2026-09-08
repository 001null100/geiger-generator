# Geiger Generator

A physical-inspired, deeply adjustable Geiger-counter sound instrument, built on **null-clap**. Native CLAP for **Windows x64 and Linux x86_64**, with a dark indigo / amber instrument panel. JUCE is used only for drawing, controls and native windows.

## Install and play

Download a platform ZIP from [Releases](https://github.com/001null100/geiger-generator/releases), extract `GeigerGenerator.clap`, and put it in a CLAP folder scanned by your host. Typical locations are `C:\Program Files\Common Files\CLAP` on Windows and `~/.clap` on Linux. Rescan plug-ins, then add **Geiger Generator** to an instrument track. It has no audio input and does not need another synthesizer.

The default **Free running** mode makes clicks immediately while the host processes the instrument. Start with **Bench / classic**, turn **Intensity**, then explore the **Click model** and **Speaker** selectors. The output starts at -12 dB. **Silence** switches Power off; switch Power back on to resume. Start at a comfortable monitoring level: sparse transients and dense noise have very different perceived loudness.

**Transport** mode follows host Play. **MIDI gate** mode generates a stream while notes are held; velocity scales its intensity. Sustain and all-notes-off are supported. Enable MIDI pitch to transpose the circuit, with MIDI note 60 neutral. The **Test click** button auditions the current circuit without waiting for a random count; Power must be on.

## Sound controls

There are **37 host parameters**, with native-unit automation, monophonic modulation where appropriate, project-state persistence and three eight-knob remote-control pages.

| Area | Controls |
| --- | --- |
| Radiation field | Intensity (0-12,000 incoming counts/s), background, randomness, tube dead time, recovery, afterpulses, clusters, wander and wander speed, source motion depth/rate |
| Click circuit | Six click models, pulse width, pitch, decay, sharpness, crackle, body and per-event variation |
| Speaker | Five transducers, enclosure resonance, tone, drive, hiss, 50/60 Hz hum, stereo scatter, listening distance, density compensation and output |
| Performance | Free / transport / MIDI gate, gate attack/release, optional MIDI pitch, seed, restart-on-play and power |

**Click models:** Classic tube, Pocket piezo, Vintage counter, Relay snap, Soft tick, Glass particle.

**Speakers:** Direct circuit, Small speaker, Piezo disc, Metal enclosure, Field radio. They can be combined freely with every click model.

**Presets:** Bench / classic, Pocket survey, Vintage civil defense, Hot zone, Glass particles, Relay rain, Drifting field, Rhythmic dosimeter, Soft dust. Preset selection preserves output level, power, run mode and seed. These names describe sounds, not radiation safety categories.

## The panel

The large intensity dial spans quiet individual ticks through dense crackle. **Probe playground** is a two-dimensional performance surface: left/right changes source intensity; up/down changes listening distance. The meter reports accepted counts per second, actual tube losses, and a recent output peak trace. It is fed by the audio engine, not a decorative random animation.

Four tabs keep the controls legible. Double-click knobs to restore defaults, type values below them, or drag vertically. Keyboard focus is visible. Hover for short help. **Calm display** removes the activity glow while keeping useful readouts. The editor resizes from 900 x 660 logical pixels and converts host physical-pixel sizes at high DPI.

## What is physically inspired?

A GM tube produces an electrical discharge when an ionizing event is detected; the readout circuit and speaker make that event audible. GM pulse height is not a measurement of the incident particle's energy. Higher source intensity primarily changes the *number and spacing* of clicks. Quenching and dead time limit how many arrivals can be counted. See the primary sources and equations in [SOUND_MODEL.md](docs/SOUND_MODEL.md).

At 100% Randomness, independent arrivals follow exponential waiting times. Non-extendable dead time and a recovery envelope are applied before the pulse circuit. Electrical transients excite damped resonances, then pass through a speaker/case response. Mono is the default. Width, rhythmic timing, clouds, exaggerated afterpulses and glass/relay voices deliberately go beyond a normal survey meter.

This is a **sound-design instrument**, not a radiation detector, dosimeter, calibrated simulator or component-level circuit emulation. Counts/s are simulated event rates, not dose units. The meter must not be used for safety decisions.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

The CLAP is written to `build/clap/GeigerGenerator.clap`. Linux requires X11, Xext, Xrandr, Xinerama, Xcursor, Xcomposite, fontconfig and FreeType development packages; see the CI workflow for the exact package list. Windows requires a recent Visual Studio C++ toolchain. Dependencies are pinned in CMake.

The pure DSP and audio example renderer can be built offline without JUCE or CLAP:

```sh
cmake -S . -B build-dsp -DGEIGER_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release --output-on-failure
./build-dsp/GeigerRender audio-demos
```

See [TESTING.md](docs/TESTING.md) for automated coverage, limitations and the manual Bitwig checklist. The build workflow publishes a pre-release only after **both platforms** build, pass DSP/host tests, render UI smoke images and pass clap-validator. Source commits alone are not proof of a usable release.

## Platform notes

Windows embeds a Win32 editor. Linux embeds an X11 editor and requires the host's standard CLAP timer-support extension to dispatch JUCE events on the host main thread. XWayland may be needed in a Wayland desktop. No native Wayland or macOS build is provided. Host buffers may be 32- or 64-bit; the engine emits float samples into either format. The instrument is not tempo-synchronized: source motion is specified in Hz.

## Source and licence

New project code is AGPL-3.0-only; see [LICENSE](LICENSE). null-clap and CLAP retain their own licences. JUCE is used under its AGPLv3 option. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) identifies pinned dependencies and their licence texts. Matching complete project source is available at the exact GitHub release commit, with pinned dependency sources reproducible through CMake.
